#include "boot.h"
#include "stm32f3xx.h"
#include "stm32f3xx_ll_bus.h"
#include "stm32f3xx_ll_gpio.h"
#include "stm32f3xx_ll_rcc.h"
#include "stm32f3xx_ll_utils.h"

int main(void)
{
  DingoHandoffInit();

  HAL_Init();

  LL_RCC_HSE_Enable();
  while (!LL_RCC_HSE_IsReady())
    ;
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSE);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSE)
    ;
  LL_SetSystemCoreClock(BOOT_CPU_SYSTEM_SPEED_KHZ * 1000u);

  BootInit();
  while (1)
    BootTask();
}

void HAL_MspInit(void)
{
  LL_GPIO_InitTypeDef gpio = {0};

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN);

  // CAN RX PA11, TX PA12
  gpio.Pin = LL_GPIO_PIN_11 | LL_GPIO_PIN_12;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  gpio.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOA, &gpio);
}

// Hand the app a reset-like clock and pin state
void HAL_MspDeInit(void)
{
  LL_RCC_DeInit();
  LL_GPIO_DeInit(GPIOA);
  LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_CAN);
  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
}

// HAL_Init() starts SysTick, OpenBLT polls its own timer
void SysTick_Handler(void)
{
}
