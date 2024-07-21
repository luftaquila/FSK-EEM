#ifndef CORE_INC_CONFIG_H
#define CORE_INC_CONFIG_H

#include <stdint.h>
#include <stdio.h>

#include "usbd_cdc_if.h"

/******************************************************************************
 * debug serial configuration
 */
#define DEBUG_MODE
#define MAX_LEN_DEBUG_STR 256

extern char debug_buffer[];

// Debug print function
#ifdef DEBUG_MODE
#define DEBUG_MSG(fmt, ...)                                                    \
  sprintf(debug_buffer, fmt, ##__VA_ARGS__);                                   \
  CDC_Transmit_FS((uint8_t *)debug_buffer, strlen(debug_buffer));              \
  HAL_Delay(10); // TODO: implement buffer instead of busy waiting
#else
#define DEBUG_MSG(fmt, ...)
#endif

/******************************************************************************
 * peripheral configuration
 */

#endif /* CORE_INC_CONFIG_H */
