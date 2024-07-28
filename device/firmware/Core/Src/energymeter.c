#include <stdint.h>

#include "energymeter.h"
#include "ff.h"
#include "main.h"
#include "ringbuffer.h"
#include "rtc.h"

/******************************************************************************
 * global variables
 *****************************************************************************/
/* boot time from the start of the month in millisecond */
uint32_t boot_time;

/* SD card write buffer */
ring_buffer_t sd_buffer;
uint8_t sd_buffer_arr[1 << 10]; // 1KB

/* RF transmit buffer */
ring_buffer_t rf_buffer;
uint8_t rf_buffer_arr[1 << 10]; // 1KB

/* device id from flash */
uint32_t device_id = DEVICE_ID_INVALID;

/* ERROR STATUS */
uint32_t error_status = EEM_NO_ERROR;

/* USB CDC debug print buffer */
char debug_buffer[MAX_LEN_DEBUG_STR];

/******************************************************************************
 * EEM record mode
 *****************************************************************************/
void mode_energymeter(void) {
  /* read boot time */
  datetime boot;
  rtc_read(&boot);

  /* read device id from the flash */
  device_id_t *devid;
  devid = (device_id_t *)FLASH_TARGET_PAGE;

  if (devid->canary != FLASH_CANARY_DEVICE_ID) {
    BIT_SET(error_status, EEM_ERR_INVALID_ID);
  } else {
    device_id = devid->id;
  }

  char path[36];
  sprintf(path, "%05ld 20%02d-%02d-%02d-%02d-%02d-%02d.log", device_id,
          boot.year, boot.month, boot.day, boot.hour, boot.minute, boot.second);

  boot_time = boot.day * 86400000 + boot.hour * 3600000 + boot.minute * 60000 +
              boot.second * 1000;

  /* init ring buffers */
  ring_buffer_init(&sd_buffer, (char *)sd_buffer_arr, sizeof(sd_buffer_arr));
  ring_buffer_init(&rf_buffer, (char *)rf_buffer_arr, sizeof(rf_buffer_arr));

  /* init telemetry */
#if RF_ENABLED
  // TODO
#endif

  /* init fatfs and log file */
  FATFS fat;

  if (f_mount(&fat, "", 1) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
  }

  FIL fp;

  if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
  };

  UINT a;
  f_write(&fp, "devid", 5, &a);
  f_close(&fp);

  /* start 100ms timer */
  HAL_TIM_Base_Start_IT(&TIMER_100ms);

  while (1) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    HAL_Delay(500);
  }
}

/******************************************************************************
 * EEM usb data dump mode
 *****************************************************************************/
void mode_usb(void) {
  while (1) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    HAL_Delay(1000);
  }
}
