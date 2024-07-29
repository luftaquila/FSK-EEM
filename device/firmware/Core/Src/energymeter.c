#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "adc.h"
#include "ff.h"
#include "main.h"
#include "rtc.h"

#include "energymeter.h"
#include "ringbuffer.h"

/******************************************************************************
 * global variables
 *****************************************************************************/
/* boot time from the start of the month in millisecond */
uint32_t boot_time;

/* random seed from the systick clock */
uint32_t seed = 0;

/* SD card write buffer */
ring_buffer_t sd_buffer;
uint8_t sd_buffer_arr[1 << 10]; // 1KB

/* RF transmit buffer */
ring_buffer_t rf_buffer;
uint8_t rf_buffer_arr[1 << 10]; // 1KB

/* device id from flash */
uint32_t device_id = DEVICE_ID_INVALID;

/* timer flag */
uint32_t timer_flag = 0;

/* analog flag and buffer */
uint32_t adc_flag = 0;
uint32_t adc_value[ADC_CH_MAX];
float adc_voltage[ADC_CH_MAX];

/* ERROR STATUS */
uint8_t error_status = 0;

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
    HAL_GPIO_WritePin(LED_SD_GPIO_Port, LED_SD_Pin, GPIO_PIN_RESET);
  }

  FIL fp;

  if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
    HAL_GPIO_WritePin(LED_SD_GPIO_Port, LED_SD_Pin, GPIO_PIN_RESET);
  };

  /* set random seed from the systick */
  srand(SysTick->VAL);

  /* start ADC calibration; RM0008 11.4 ADC Calibration */
  while(HAL_ADCEx_Calibration_Start(&ADC) != HAL_OK);

  /* start 100ms timer */
  HAL_TIM_Base_Start_IT(&TIMER_100ms);

  while (1) {
    if (adc_flag) {
      adc_flag = FALSE;
    }

    if (BIT_CHECK(timer_flag, TIMER_FLAG_100ms)) {
      BIT_CLEAR(timer_flag, TIMER_FLAG_100ms);
      HAL_ADC_Start_DMA(&ADC, adc_value, 5);
    }

    if (BIT_CHECK(timer_flag, TIMER_FLAG_random)) {
      BIT_CLEAR(timer_flag, TIMER_FLAG_random);
    }

    if (BIT_CHECK(timer_flag, TIMER_FLAG_1000ms)) {
      BIT_CLEAR(timer_flag, TIMER_FLAG_1000ms);
    }
  }
}

/******************************************************************************
 * EEM energymeter mode 100ms periodic job
 *****************************************************************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  static uint32_t counter = 0;
  ++counter;

  BIT_SET(timer_flag, TIMER_FLAG_100ms);

  static uint32_t random_counter = 7;

  if (counter == random_counter) {
    // set next random event to 700~900ms
    // to reduce prevent collisions from multiple transmitters
    // i know rand() is a bad PRNG, but that is not important
    random_counter += 7 + rand() % 3;

    if (random_counter > 10) {
      random_counter -= 10;
    }

    BIT_SET(timer_flag, TIMER_FLAG_random);
  }

  if (counter == 10) {
    counter = 0;
    BIT_SET(timer_flag, TIMER_FLAG_1000ms);
  }

  /* flash status led; OK 1 Hz, Error 10 Hz */
  if (error_status || !counter) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
  }
}

/******************************************************************************
 * EEM energymeter mode ADC convert complete job
 *****************************************************************************/
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
  // VDDA calculation
  adc_voltage[ADC_CH_VREFINT] = (float)((1 << ADC_RESOLUTION) - 1) * ADC_VREFINT / (float)(adc_value[ADC_CH_VREFINT]);

  // calibrate channels with calcualted VDDA
  for (int i = ADC_CH_HV_VOLTAGE; i < ADC_CH_MAX; i++) {
    adc_voltage[i] = (float)((1 << ADC_RESOLUTION) - 1) * (float)(adc_value[i]) / adc_voltage[ADC_CH_VREFINT];
  }

  #if TEMPSENSOR_ENABLED
  // RM0008 11.10 ADC Temperature sensor
  adc_voltage[ADC_CH_TEMPERATURE] = (((ADC_TEMP_V25 - adc_voltage[ADC_CH_TEMPERATURE]) / ADC_TEMP_AVG_SLOPE) + 25.0) * 10.0;
  #endif

  adc_flag = TRUE;
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
