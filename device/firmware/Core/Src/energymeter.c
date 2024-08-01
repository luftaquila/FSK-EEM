#include <stdlib.h>

#include "adc.h"
#include "crc.h"
#include "ff.h"
#include "main.h"
#include "rtc.h"

#include "energymeter.h"

/******************************************************************************
 * global variables
 *****************************************************************************/
/* boot time from the start of the month in millisecond */
uint32_t boot_time;

/* random seed */
uint32_t seed = 0;

/* log item */
log_item_t syslog = { .id = DEVICE_ID_INVALID, .magic = MAGIC_PACKET_END };

/* timer flag */
uint32_t timer_flag = FALSE;

/* analog flag and buffer */
uint32_t adc_flag = FALSE;
uint32_t adc_value[ADC_CH_MAX];
float adc_voltage[ADC_CH_MAX];

/* ERROR STATUS */
uint8_t error_status = FALSE;

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
  device_id_t *devid = (device_id_t *)FLASH_TARGET_PAGE;

  if (devid->canary != FLASH_CANARY_DEVICE_ID) {
    BIT_SET(error_status, EEM_ERR_INVALID_ID);
  } else {
    syslog.id = devid->id;
  }

  char path[36];
  sprintf(path, "%05u-20%02d-%02d-%02d-%02d-%02d-%02d.log", syslog.id,
          boot.year, boot.month, boot.day, boot.hour, boot.minute, boot.second);

  boot_time = boot.day * 86400000 + boot.hour * 3600000 + boot.minute * 60000 +
              boot.second * 1000;

  /* init telemetry */
#if RF_ENABLED
  // TODO
#endif

  /* init fatfs and log file */
  FATFS fat;

  if (f_mount(&fat, "", 1) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
    HAL_GPIO_WritePin(LED_SD_ERR_GPIO_Port, LED_SD_ERR_Pin, GPIO_PIN_RESET);
  }

  // it is not important that this failed or not
  (void)f_setlabel(VOLUME_LABEL);

  FIL fp;

  if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
    HAL_GPIO_WritePin(LED_SD_ERR_GPIO_Port, LED_SD_ERR_Pin, GPIO_PIN_RESET);
  };

  /* set random seed from the systick */
  srand(SysTick->VAL);

  /* start ADC calibration; RM0008 11.4 ADC Calibration */
  while (HAL_ADCEx_Calibration_Start(&ADC) != HAL_OK) {}

  /* start 100ms timer */
  HAL_TIM_Base_Start_IT(&TIMER_20ms);

  /* mark device operating */
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);

  while (1) {
    // adc conversion complete flag set
    if (adc_flag) {
      syslog.time = boot_time + HAL_GetTick();
      syslog.payload.log_item_report.hv_v = (uint16_t)(adc_voltage[ADC_CH_HV_VOLTAGE] * 10000);
      syslog.payload.log_item_report.hv_c = (uint16_t)(adc_voltage[ADC_CH_HV_CURRENT] * 10000);
      syslog.payload.log_item_report.lv_v = (uint16_t)(adc_voltage[ADC_CH_LV_VOLTAGE] * 10000);
      syslog.type = LOG_TYPE_REPORT;
      syslog.checksum = 0;
      syslog.checksum = HAL_CRC_Calculate(&hcrc, (uint32_t *)&syslog, sizeof(syslog));

      UINT written;

      // write directly into the fatfs buffer; will take mostly 5us, or sometimes 40us
      if (f_write(&fp, &syslog, sizeof(syslog), &written) != FR_OK) {
        BIT_SET(error_status, EEM_ERR_SD_CARD);
        HAL_GPIO_WritePin(LED_SD_ERR_GPIO_Port, LED_SD_ERR_Pin, GPIO_PIN_RESET);
      }

      adc_flag = FALSE;
    }

    // 20ms timer event flag set
    if (BIT_CHECK(timer_flag, TIMER_FLAG_20ms)) {
      BIT_CLEAR(timer_flag, TIMER_FLAG_20ms);
      HAL_ADC_Start_DMA(&ADC, adc_value, ADC_CH_MAX);
    }

    // 700~840ms random timer event flag set
    if (BIT_CHECK(timer_flag, TIMER_FLAG_random)) {
      BIT_CLEAR(timer_flag, TIMER_FLAG_random);
    }

    // 960ms timer event flag set
    if (BIT_CHECK(timer_flag, TIMER_FLAG_960ms)) {
      BIT_CLEAR(timer_flag, TIMER_FLAG_960ms);

      // write fatfs buffer to the file; will take mostly ~5ms, worst 12ms
      if (f_sync(&fp) != FR_OK) {
        BIT_SET(error_status, EEM_ERR_SD_CARD);
        HAL_GPIO_WritePin(LED_SD_ERR_GPIO_Port, LED_SD_ERR_Pin, GPIO_PIN_RESET);
      }
    }
  }
}

/******************************************************************************
 * EEM energymeter mode 100ms periodic job
 *****************************************************************************/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  static uint32_t cnt = 0;
  ++cnt;

  BIT_SET(timer_flag, TIMER_FLAG_20ms);

  static uint32_t random_cnt = TIM_CNT_RAND_BASE;

  if (cnt == random_cnt) {
    // set next random event to 700~840ms to reduce collisions
    // yes. rand() is a bad PRNG. but that is not important, maybe
    random_cnt += TIM_CNT_RAND_BASE + (rand() & 0b0111);

    if (random_cnt > TIM_CNT_MAX) {
      random_cnt -= TIM_CNT_MAX;
    }

    BIT_SET(timer_flag, TIMER_FLAG_random);
  }

  if (cnt == TIM_CNT_MAX) {
    cnt = 0;
    BIT_SET(timer_flag, TIMER_FLAG_960ms);
  }

  /* flash status led; OK 1 Hz, Error 6.25 Hz */
  if (!cnt || (error_status && !(cnt & 0b0111))) {
    HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
  }
}

/******************************************************************************
 * EEM energymeter mode ADC convert complete job
 *****************************************************************************/
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  // VDDA calculation
  adc_voltage[ADC_CH_VREFINT] = (float)((1 << ADC_RESOLUTION) - 1) * ADC_VREFINT / (float)(adc_value[ADC_CH_VREFINT]);

  // calibrate channels with calcualted VDDA
  for (int i = ADC_CH_HV_VOLTAGE; i < ADC_CH_MAX; i++) {
    adc_voltage[i] = (float)adc_value[i] / (float)((1 << ADC_RESOLUTION) - 1) * adc_voltage[ADC_CH_VREFINT];
  }

  // RM0008 11.10 ADC Temperature sensor
  adc_voltage[ADC_CH_TEMPERATURE] = (((ADC_TEMP_V25 - adc_voltage[ADC_CH_TEMPERATURE]) / ADC_TEMP_AVG_SLOPE) + 25.0);

  adc_flag = TRUE;
}
