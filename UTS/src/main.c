#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#define PACKET_SIZE         50U     // Ukuran buffer teks per baris
#define FIFO_CAPACITY       20U     // Kapasitas antrean ring buffer
#define DEBOUNCE_TIME_MS    50U     // Debounce tombol eksternal: 50 ms

typedef struct {
    char buffer[FIFO_CAPACITY][PACKET_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t count;
} PacketFIFO_t;

ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim2;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef  hdma_usart1_tx;

volatile uint16_t adc_dma_buf[3];           // Buffer DMA 3 Potensio (PA1, PA2, PA3)
char              uart_tx_buf[PACKET_SIZE]; // Buffer aktif yang dikirim DMA TX
PacketFIFO_t      tx_fifo = {0};
volatile uint8_t  uart_tx_busy = 0;

/* Variabel Penampung Karakter Keyboard dari PC (UART RX) */
uint8_t           rx_byte = 0;

/* Variabel Push Button Eksternal */
volatile uint32_t last_btn_tick = 0;
volatile uint32_t btn_press_counter = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);

uint8_t FIFO_Push(PacketFIFO_t *fifo, const char *msg);
uint8_t FIFO_Pop(PacketFIFO_t *fifo, char *msg);

/*FUNGSI FIFO RING BUFFER*/
uint8_t FIFO_Push(PacketFIFO_t *fifo, const char *msg)
{
    uint8_t status = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (fifo->count < FIFO_CAPACITY) {
        strncpy(fifo->buffer[fifo->head], msg, PACKET_SIZE - 1);
        fifo->buffer[fifo->head][PACKET_SIZE - 1] = '\0';
        fifo->head = (fifo->head + 1U) % FIFO_CAPACITY;
        fifo->count++;
        status = 1;
    }

    if (!primask) {
        __enable_irq();
    }
    return status;
}

uint8_t FIFO_Pop(PacketFIFO_t *fifo, char *msg)
{
    uint8_t status = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (fifo->count > 0U) {
        strncpy(msg, fifo->buffer[fifo->tail], PACKET_SIZE);
        fifo->tail = (fifo->tail + 1U) % FIFO_CAPACITY;
        fifo->count--;
        status = 1;
    }

    if (!primask) {
        __enable_irq();
    }
    return status;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    /* 1. Mulai dengarkan input 1-byte keyboard dari PC via Interrupt */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    /* 2. Mulai DMA ADC Circular 3 Channel (PA1, PA2, PA3) */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buf, 3);

    /* 3. Mulai Hardware Timer 2 TRGO (50 Hz / 20 ms) */
    HAL_TIM_Base_Start(&htim2);

    while (1)
    {
        /* Non-Blocking Arbiter untuk transmisi serial */
        if ((uart_tx_busy == 0U) && (tx_fifo.count > 0U))
        {
            if (FIFO_Pop(&tx_fifo, uart_tx_buf) == 1U)
            {
                uart_tx_busy = 1U;
                uint16_t len = (uint16_t)strlen(uart_tx_buf);
                if (HAL_UART_Transmit_DMA(&huart1, (uint8_t*)uart_tx_buf, len) != HAL_OK)
                {
                    uart_tx_busy = 0U;
                }
            }
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        char resp[PACKET_SIZE] = {0};

        switch (rx_byte)
        {
            case 'U':
            case 'u':
                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12); // Toggle LED Hijau
                snprintf(resp, sizeof(resp), ">>> [PC] Tombol 'U' -> Toggle LED Hijau\r\n");
                FIFO_Push(&tx_fifo, resp);
                break;

            case 'I':
            case 'i':
                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13); // Toggle LED Oranye
                snprintf(resp, sizeof(resp), ">>> [PC] Tombol 'I' -> Toggle LED Oranye\r\n");
                FIFO_Push(&tx_fifo, resp);
                break;

            case 'O':
            case 'o':
                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14); // Toggle LED Merah
                snprintf(resp, sizeof(resp), ">>> [PC] Tombol 'O' -> Toggle LED Merah\r\n");
                FIFO_Push(&tx_fifo, resp);
                break;

            case 'P':
            case 'p':
                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15); // Toggle LED Biru
                snprintf(resp, sizeof(resp), ">>> [PC] Tombol 'P' -> Toggle LED Biru\r\n");
                FIFO_Push(&tx_fifo, resp);
                break;

            default:
                break;
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        uint32_t current_tick = HAL_GetTick();
        if ((current_tick - last_btn_tick) >= DEBOUNCE_TIME_MS)
        {
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET)
            {
                last_btn_tick = current_tick;
                btn_press_counter++;

                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);

                char msg[PACKET_SIZE];
                snprintf(msg, sizeof(msg), ">>> [BTN] Ditekan! Total: %lu\r\n", btn_press_counter);
                FIFO_Push(&tx_fifo, msg);
            }
        }
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        uint16_t ch1 = adc_dma_buf[0] & 0x0FFFU; // PA1
        uint16_t ch2 = adc_dma_buf[1] & 0x0FFFU; // PA2
        uint16_t ch3 = adc_dma_buf[2] & 0x0FFFU; // PA3

        char msg[PACKET_SIZE];
        snprintf(msg, sizeof(msg), "P1:%04u | P2:%04u | P3:%04u\r\n", ch1, ch2, ch3);
        FIFO_Push(&tx_fifo, msg);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart_tx_busy = 0U;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart_tx_busy = 0U;
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);

    /* 4 LED Onboard (PD12=Hijau, PD13=Oranye, PD14=Merah, PD15=Biru) */
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Push Button PB0: Active-Low dengan Internal Pull-Up */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Input Analog 3 Potensio: PA1, PA2, PA3 */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 TX (PB6) dan RX (PB7) */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();

    /* DMA2 Stream 0 untuk ADC1 */
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    /* DMA2 Stream 7 untuk USART1_TX */
    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 1, 1);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
}

static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    __HAL_RCC_ADC1_CLK_ENABLE();

    hdma_adc1.Instance = DMA2_Stream0;
    hdma_adc1.Init.Channel = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_adc1);

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ENABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 3;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    HAL_ADC_Init(&hadc1);

    sConfig.Channel = ADC_CHANNEL_1;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = 2;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    sConfig.Channel = ADC_CHANNEL_3;
    sConfig.Rank = 3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 8400 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 200 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim2);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    hdma_usart1_tx.Instance = DMA2_Stream7;
    hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart1_tx);

    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    /* Aktifkan mode TX dan RX */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    /* Untuk memproses Interrupt RX dan Callback TX */
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 2);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}