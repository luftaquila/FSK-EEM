#ifndef CORE_INC_ENERGYMETER_H
#define CORE_INC_ENERGYMETER_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "usbd_cdc_if.h"

#define TRUE 1
#define FALSE 0

/******************************************************************************
 * module configuration
 */
#define DEBUG_ENABLED  TRUE
#define RF_ENABLED     FALSE

extern uint32_t boot_time;

/******************************************************************************
 * module error configuration
 */
extern uint32_t error_status;

typedef enum {
  EEM_NO_ERROR,
  EEM_ERR_SD,
} error_type_t;


/******************************************************************************
 * logger configuration
 */
#define FLASH_CANARY_DEVICE_ID 0xBADACAFE;

#define DEVICE_ID_INVALID      0xFFFF;
#define DEVICE_ID_BROADCAST    0xFFFE;

typedef enum {
  LOG_TYPE_REPORT,  // 10Hz HV / LV report 
  LOG_TYPE_EVENT,   // instant event record
  LOG_TYPE_COMMAND, // device id / rtc set cmd
  LOG_TYPE_ERROR,   // device error status
} log_type_t;

typedef struct {
  uint16_t hv_v;    // HV bus voltage
  uint16_t hv_c;    // HV bus current
  uint16_t lv_v;    // LV bus voltage
} log_item_report_t;

typedef struct {
  char msg[6];      // instant event message string
} log_item_event_t;

typedef struct {
  uint16_t id;      // new device id
  uint32_t time;    // RTC time fix
} log_item_command_t;

typedef struct {
  uint32_t error_time; // error event time
  uint16_t _reserved;  // reserved for future use
} log_item_error_t;

typedef struct {
  uint32_t time;      // millisecond eslaped from the start of the boot month
  union {
    log_item_report_t  log_item_report;
    log_item_event_t   log_item_event;
    log_item_command_t log_item_command;
    log_item_error_t   log_item_error;
  } payload;          // 6 byte log payload
  uint8_t type;       // log type
  uint8_t _reserved;  // reserved for future use
  uint16_t id;        // my device id
  uint16_t checksum;  // CRC-16 checksum
} log_item_t;

typedef struct {
  uint16_t target;   // target device id
  union {
    log_item_event_t   log_item_event;
    log_item_command_t log_item_command;
  } payload;         // remote command payload
  uint8_t type;      // log type
  uint8_t _reserved; // reserved for future use
  uint16_t checksum; // CRC-16 checksum
} remote_cmd_t;

/******************************************************************************
 * telemetry configuration
 */


/******************************************************************************
 * peripheral configuration
 */
#define TIMER_100ms htim1

#define UART_CONS huart1

#define SPI_SD hspi1
#define SPI_RF hspi2


/******************************************************************************
 * debug configuration
 */
#define MAX_LEN_DEBUG_STR 256

extern char debug_buffer[];

// Debug print function
#if DEBUG_ENABLED
static inline void debug_print(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsprintf(debug_buffer, fmt, args);

  CDC_Transmit_FS((uint8_t *)debug_buffer, strlen(debug_buffer));
  HAL_Delay(10); // TODO: implement buffer instead of busy waiting
}

#define DEBUG_MSG(fmt, ...) debug_print(fmt, ##__VA_ARGS__)
#else
#define DEBUG_MSG(fmt, ...)
#endif


/******************************************************************************
 * function prototypes
 */
void mode_energymeter(void);
void mode_usb(void);

#endif /* CORE_INC_ENERGYMETER_H */
