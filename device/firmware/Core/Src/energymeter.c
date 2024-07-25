#include "main.h"
#include "rtc.h"
#include "stm32f1xx_hal.h"
#include "types.h"
#include "energymeter.h"

char debug_buffer[MAX_LEN_DEBUG_STR];

void mode_energymeter(void) {
  datetime boot;

  rtc_read(&boot);

  while (1) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
  }
}

void mode_usb(void) {
  while (1) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(1000);
  }
}

