#include "stm32f4xx_hal.h"

void Init_OnBoard_LEDs(void);
void configure_Button(void);
void Delay_ms(volatile int time_ms);

int main(void)
{
Init_OnBoard_LEDs(); 
configure_Button(); 
GPIO_PinState state; 
   while(1)
   {
		
state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    if(state)
	{
HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_SET);
	}
    else
       {
HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);
       }
   }
}

void Init_OnBoard_LEDs(void)
{
	 __HAL_RCC_GPIOD_CLK_ENABLE(); 
	GPIO_InitTypeDef BoardLEDs;
	BoardLEDs.Mode = GPIO_MODE_OUTPUT_PP;
	BoardLEDs.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &BoardLEDs); 
}

void configure_Button(void)
{
	__HAL_RCC_GPIOA_CLK_ENABLE(); 
	GPIO_InitTypeDef PushButton;  
	PushButton.Mode = GPIO_MODE_INPUT; 
	PushButton.Pin = GPIO_PIN_0;  
	PushButton.Pull = GPIO_NOPULL; 
	HAL_GPIO_Init(GPIOA, &PushButton); 
}

void Delay_ms(volatile int time_ms)
{
	      int j;
        for(j = 0; j < time_ms*4000; j++)
            {}
}