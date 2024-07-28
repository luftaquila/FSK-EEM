#include "diskio.h"
#include "ff.h"
#include "rtc.h"
#include "tim.h"

#include "energymeter.h"
#include "main.h"
#include "ringbuffer.h"

/* boot time from the start of the month in millisecond */
uint32_t boot_time;

/* SD card write buffer */
ring_buffer_t sd_buffer;
uint8_t sd_buffer_arr[1 << 10]; // 1KB

/* RF transmit buffer */
ring_buffer_t rf_buffer;
uint8_t rf_buffer_arr[1 << 10]; // 1KB

/* ERROR STATUS */
uint32_t error_status = EEM_NO_ERROR;

/* USB CDC debug print buffer */
char debug_buffer[MAX_LEN_DEBUG_STR];


void mode_energymeter(void) {
  /* read boot time */
  datetime boot;
  rtc_read(&boot);

  char path[30];
  sprintf(path, "20%02d-%02d-%02d-%02d-%02d-%02d.log", boot.year, boot.month,
          boot.day, boot.hour, boot.minute, boot.second);

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
    error_status = EEM_ERR_SD;
  }

  FIL fp;

  if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    error_status = EEM_ERR_SD;
  };


  /* start 100ms timer */
  HAL_TIM_Base_Start_IT(&TIMER_100ms);


  while (1) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    HAL_Delay(500);
  }
}


void mode_usb(void) {
  while (1) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    HAL_Delay(1000);
  }
}
