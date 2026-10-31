#include "boot.h"
#include "stm32g4xx.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_utils.h"

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
  LL_RCC_SetFDCANClockSource(LL_RCC_FDCAN_CLKSOURCE_PCLK1);

  BootInit();
  while (1)
    BootTask();
}

void HAL_MspInit(void)
{
  LL_GPIO_InitTypeDef gpio = {0};

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC | LL_AHB2_GRP1_PERIPH_GPIOD);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_FDCAN);

  // Transceiver out of standby (LINE_CAN_STANDBY low)
  gpio.Pin = LL_GPIO_PIN_12;
  gpio.Mode = LL_GPIO_MODE_OUTPUT;
  gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_12);
  LL_GPIO_Init(GPIOC, &gpio);

  // FDCAN1 RX PD0, TX PD1
  gpio.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1;
  gpio.Mode = LL_GPIO_MODE_ALTERNATE;
  gpio.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = LL_GPIO_AF_9;
  LL_GPIO_Init(GPIOD, &gpio);
}

// Hand the app a reset-like clock and pin state
void HAL_MspDeInit(void)
{
  LL_RCC_DeInit();
  LL_GPIO_DeInit(GPIOC);
  LL_GPIO_DeInit(GPIOD);
  LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_FDCAN);
  LL_AHB2_GRP1_DisableClock(LL_AHB2_GRP1_PERIPH_GPIOC | LL_AHB2_GRP1_PERIPH_GPIOD);
}

// HAL_Init() starts SysTick, OpenBLT polls its own timer
void SysTick_Handler(void)
{
}
