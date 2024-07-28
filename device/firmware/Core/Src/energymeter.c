#include "main.h"
#include "diskio.h"
#include "ff.h"
#include "rtc.h"

#include "energymeter.h"

char debug_buffer[MAX_LEN_DEBUG_STR];

void mode_energymeter(void) {
  datetime boot;
  rtc_read(&boot);

  char path[30];
  sprintf(path, "20%02d-%02d-%02d-%02d-%02d-%02d.log", boot.year, boot.month,
          boot.day, boot.hour, boot.minute, boot.second);

  DEBUG_MSG("%s\n", path);

  FATFS fat;
  FIL fp;

  disk_initialize((BYTE)0);

  DEBUG_MSG("disk init\n");

  int ret = f_mount(&fat, "", 1);

  DEBUG_MSG("mount: %d\n", ret);

  ret = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE);

  DEBUG_MSG("open: %d\n", ret);

  UINT a;

  ret = f_write(&fp, "hihello", 7, &a);

  DEBUG_MSG("write: %d, %d\n", ret, a);

  ret = f_close(&fp);

  DEBUG_MSG("close: %d\n", ret);

  while (1) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(500);
  }
}

void mode_usb(void) {
  while (1) {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(1000);
  }
}
