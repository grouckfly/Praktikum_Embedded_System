/**
  ******************************************************************************
  * @file    src/main.c
  * @brief   Implementasi Multitasking Preemptive FreeRTOS pada STM32F407 Discovery
  *          Mengontrol 4 LED Onboard (PD12, PD13, PD14, PD15) secara konkuren
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

/* Definisi Pin LED Onboard STM32F4-Discovery */
#define LED_GREEN_PIN    GPIO_PIN_12
#define LED_ORANGE_PIN   GPIO_PIN_13
#define LED_RED_PIN      GPIO_PIN_14
#define LED_BLUE_PIN     GPIO_PIN_15
#define LED_GPIO_PORT    GPIOD

/* Prototype Fungsi Konfigurasi Hardware */
static void SystemClock_Config(void);
static void GPIO_Init(void);

/* Prototype Fungsi Task FreeRTOS */
static void Task_LED_Green(void *pvParameters);
static void Task_LED_Orange(void *pvParameters);
static void Task_LED_Red(void *pvParameters);
static void Task_LED_Blue(void *pvParameters);

/* Deklarasi handler SysTick internal FreeRTOS port (ARM Cortex-M4F) */
extern void xPortSysTickHandler(void);

/**
  * @brief  SysTick Handler terpadu antara STM32 HAL dan FreeRTOS Kernel.
  *         - HAL_IncTick() wajib dipanggil setiap 1 ms untuk mengupdate uwTick
  *           agar timeout dan fungsi internal driver HAL tidak membeku (freeze).
  *         - xPortSysTickHandler() memajukan FreeRTOS tick dan menangani preemptive
  *           task switching setelah scheduler aktif.
  */
void SysTick_Handler(void)
{
    HAL_IncTick();

    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

/**
  * @brief  Titik masuk utama aplikasi (Main Entry Point)
  */
int main(void)
{
    /* 1. Inisialisasi HAL Driver: Flash prefetch, instruction/data cache, dan NVIC Group Priority */
    HAL_Init();

    /* 2. Inisialisasi Sistem Clock ke 168 MHz menggunakan kristal eksternal HSE 8 MHz */
    SystemClock_Config();

    /* 3. Inisialisasi peripheral GPIO Port D untuk 4 LED Onboard */
    GPIO_Init();

    /* 4. Pembuatan 4 Task FreeRTOS Independen dengan prioritas yang sama */
    BaseType_t xResult;

    xResult = xTaskCreate(Task_LED_Green,
                          "LED_Green",
                          configMINIMAL_STACK_SIZE,
                          NULL,
                          tskIDLE_PRIORITY + 1,
                          NULL);
    configASSERT(xResult == pdPASS);

    xResult = xTaskCreate(Task_LED_Orange,
                          "LED_Orange",
                          configMINIMAL_STACK_SIZE,
                          NULL,
                          tskIDLE_PRIORITY + 1,
                          NULL);
    configASSERT(xResult == pdPASS);

    xResult = xTaskCreate(Task_LED_Red,
                          "LED_Red",
                          configMINIMAL_STACK_SIZE,
                          NULL,
                          tskIDLE_PRIORITY + 1,
                          NULL);
    configASSERT(xResult == pdPASS);

    xResult = xTaskCreate(Task_LED_Blue,
                          "LED_Blue",
                          configMINIMAL_STACK_SIZE,
                          NULL,
                          tskIDLE_PRIORITY + 1,
                          NULL);
    configASSERT(xResult == pdPASS);

    /* 5. Memulai Kernel Scheduler FreeRTOS */
    vTaskStartScheduler();

    /* Baris ini seharusnya tidak pernah tercapai kecuali alokasi heap kernel gagal */
    while (1)
    {
    }
}

/* ========================================================================== */
/*                           IMPLEMENTASI TASK FREERTOS                       */
/* ========================================================================== */

/**
  * @brief  Task untuk LED Hijau (PD12) - Toggle setiap 250 ms
  */
static void Task_LED_Green(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(250);

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GREEN_PIN);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
  * @brief  Task untuk LED Oranye (PD13) - Toggle setiap 500 ms
  */
static void Task_LED_Orange(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500);

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_ORANGE_PIN);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
  * @brief  Task untuk LED Merah (PD14) - Toggle setiap 1000 ms (1 detik)
  */
static void Task_LED_Red(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_RED_PIN);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
  * @brief  Task untuk LED Biru (PD15) - Toggle setiap 2000 ms (2 detik)
  */
static void Task_LED_Blue(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(2000);

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_BLUE_PIN);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* ========================================================================== */
/*                     INISIALISASI HARDWARE (CLOCK & GPIO)                   */
/* ========================================================================== */

/**
  * @brief  Konfigurasi Sistem Clock STM32F407:
  *         - Sumber: HSE Crystal 8 MHz
  *         - PLL: PLL_M = 8, PLL_N = 336, PLL_P = 2, PLL_Q = 7
  *         - Frekuensi Inti (SYSCLK / HCLK): 168 MHz
  *         - Bus APB1: 42 MHz (HCLK / 4)
  *         - Bus APB2: 84 MHz (HCLK / 2)
  *         - Flash Latency: 5 Wait States (FLASH_LATENCY_5)
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Aktifkan Power Controller Clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Atur Voltage Scaling ke Scale 1 untuk mendukung frekuensi 168 MHz */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Inisialisasi Osilator HSE dan PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2; /* 336 / 2 = 168 MHz */
    RCC_OscInitStruct.PLL.PLLQ       = 7;             /* 336 / 7 = 48 MHz (USB/SDIO/RNG) */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        /* Inisialisasi osilator gagal */
        while (1);
    }

    /* Inisialisasi Bus Clock (AHB, APB1, APB2) */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1; /* HCLK  = 168 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;   /* PCLK1 = 42 MHz  */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;   /* PCLK2 = 84 MHz  */

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        /* Konfigurasi bus clock gagal */
        while (1);
    }
}

/**
  * @brief  Inisialisasi GPIO Port D Pin 12, 13, 14, 15 sebagai output push-pull
  */
static void GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Aktifkan clock peripheral GPIOD */
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* Matikan semua LED saat booting awal */
    HAL_GPIO_WritePin(LED_GPIO_PORT,
                      LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN,
                      GPIO_PIN_RESET);

    /* Konfigurasi Pin 12, 13, 14, 15: Output Push-Pull, No-Pull, Kecepatan Low */
    GPIO_InitStruct.Pin   = LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

/* ========================================================================== */
/*                          FREERTOS HOOK IMPLEMENTATIONS                     */
/* ========================================================================== */

/**
  * @brief  Hook yang dipanggil jika terjadi Stack Overflow pada suatu task
  */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;);
}

/**
  * @brief  Hook yang dipanggil jika alokasi pvPortMalloc() gagal kehabisan heap
  */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;);
}
