#include "boot.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_utils.h"

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

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_GPIOC);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_CAN1);

  // Transceiver out of standby (LINE_CAN_STANDBY low)
  gpio.Pin = LL_GPIO_PIN_12;
  gpio.Mode = LL_GPIO_MODE_OUTPUT;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_12);
  LL_GPIO_Init(GPIOC, &gpio);

  // CAN1 RX PB8, TX PB9
  gpio.Pin = LL_GPIO_PIN_8 | LL_GPIO_PIN_9;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOB, &gpio);
}

// Hand the app a reset-like clock and pin state
void HAL_MspDeInit(void)
{
  LL_RCC_DeInit();
  LL_GPIO_DeInit(GPIOB);
  LL_GPIO_DeInit(GPIOC);
  LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_CAN1);
  LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_GPIOC);
}

// HAL_Init() starts SysTick, OpenBLT polls its own timer
void SysTick_Handler(void)
{
}
