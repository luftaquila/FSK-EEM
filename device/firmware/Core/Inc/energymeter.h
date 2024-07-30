#ifndef CORE_INC_ENERGYMETER_H
#define CORE_INC_ENERGYMETER_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "usbd_cdc_if.h"

/******************************************************************************
 * basic definitions
 *****************************************************************************/
#define TRUE  (1)
#define FALSE (0)

#define BIT_SET(target, pos)    ((target) |= (1 << (pos)))
#define BIT_CLEAR(target, pos)  ((target) &= ~(1 << (pos)))
#define BIT_TOGGLE(target, pos) ((target) ^= (1 << (pos)))
#define BIT_CHECK(target, pos)  ((target) & (1 << (pos)))

/******************************************************************************
 * module configuration
 *****************************************************************************/
// enable output debug print to USB CDC on energymeter mode
#define DEBUG_ENABLED TRUE

// eanble telemetry
#define RF_ENABLED FALSE

/******************************************************************************
 * module error configuration
 *****************************************************************************/
extern uint8_t error_status;

// max 8 entries
typedef enum {
  EEM_ERR_INVALID_ID,
  EEM_ERR_SD_CARD,
  EEM_ERR_HARDFAULT,
} error_type_t;

/******************************************************************************
 * logger configuration
 *****************************************************************************/
extern uint32_t boot_time;

// flash page 63, PM0075 Table 3. Flash module organization (medium density)
#define FLASH_TARGET_PAGE      0x0800FC00
#define FLASH_CANARY_DEVICE_ID 0xBADACAFE

typedef struct {
  uint32_t canary;
  uint32_t id;
} device_id_t;

/* reserved device ids */
#define DEVICE_ID_INVALID   0xFFFF
#define DEVICE_ID_BROADCAST 0xFFFE

/* mark packet end magic byte */
#define MAGIC_PACKET_END 0xAA

/* log format definitions */
// max 8 entries
typedef enum {
  LOG_TYPE_REPORT,  // 10Hz HV / LV report
  LOG_TYPE_EVENT,   // instant event record
  LOG_TYPE_COMMAND, // device id / rtc set cmd
  LOG_TYPE_ERROR,   // device error status
} log_type_t;

typedef struct {
  uint16_t hv_v; // HV bus voltage
  uint16_t hv_c; // HV bus current
  uint16_t lv_v; // LV bus voltage
} log_item_report_t;

typedef struct {
  char msg[6]; // instant event message string
} log_item_event_t;

typedef struct {
  uint16_t id;   // new device id
  uint32_t time; // RTC time fix
} log_item_command_t;

typedef struct {
  uint32_t time;     // latest error event time
  uint8_t status;    // all present error status
  uint8_t _reserved; // reserved for future use
} log_item_error_t;

typedef struct {
  uint32_t time; // millisecond eslaped from the start of the boot month
  union {
    log_item_report_t log_item_report;
    log_item_event_t log_item_event;
    log_item_command_t log_item_command;
    log_item_error_t log_item_error;
  } payload;         // 6 byte log payload
  uint16_t id;       // device id
  uint16_t checksum; // CRC-16 checksum
  uint8_t type;      // log type
  uint8_t magic;     // magic packet end byte 0xAA
} log_item_t;

/* remote command definitions */
typedef struct {
  uint16_t target; // target device id
  union {
    log_item_event_t log_item_event;
    log_item_command_t log_item_command;
  } payload;         // remote command payload
  uint8_t type;      // log type
  uint8_t _reserved; // reserved for future use
  uint16_t checksum; // CRC-16 checksum
} remote_cmd_t;

/******************************************************************************
 * telemetry configuration
 *****************************************************************************/

/******************************************************************************
 * peripheral configuration
 *****************************************************************************/
extern TIM_HandleTypeDef htim1;

#define TIMER_20ms htim1

#define TIM_CNT_MAX       48 // 960ms
#define TIM_CNT_RAND_BASE 35 // 700ms

typedef enum {
  TIMER_FLAG_20ms,
  TIMER_FLAG_random,
  TIMER_FLAG_960ms,
} timer_flag_t;

#define UART_CONS huart1

#define SPI_SD hspi1
#define SPI_RF hspi2

#define ADC hadc1

typedef enum {
  ADC_CH_VREFINT,
  ADC_CH_HV_VOLTAGE,
  ADC_CH_HV_CURRENT,
  ADC_CH_LV_VOLTAGE,
  ADC_CH_TEMPERATURE,
  ADC_CH_MAX,
} adc_channel_t;

// RM0008 11.2 ADC main features
#define ADC_RESOLUTION 12 // bit resolution

// STM32F103x8 datasheet 5.3.4 Embedded reference voltage
#define ADC_VREFINT 1.20

// STM32F103x8 datasheet 5.3.19 Temperature sensor characteristics
#define ADC_TEMP_V25       1.43
#define ADC_TEMP_AVG_SLOPE 4.3

/******************************************************************************
 * debug configuration
 *****************************************************************************/
#define MAX_LEN_DEBUG_STR 256

extern char debug_buffer[];

// Debug print function
#if DEBUG_ENABLED
static inline void debug_print(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsprintf(debug_buffer, fmt, args);

  CDC_Transmit_FS((uint8_t *)debug_buffer, strlen(debug_buffer));
  HAL_Delay(1); // just busy wait a bit
}

  #define DEBUG_MSG(fmt, ...) debug_print(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_MSG(fmt, ...)
#endif

/******************************************************************************
 * function prototypes
 *****************************************************************************/
void mode_energymeter(void);
void mode_usb(void);

#endif /* CORE_INC_ENERGYMETER_H */
