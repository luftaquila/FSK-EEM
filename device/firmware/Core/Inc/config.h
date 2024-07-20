#ifndef CORE_INC_CONFIG_H
#define CORE_INC_CONFIG_H

#include <stdint.h>
#include <stdio.h>

#include "usbd_cdc_acm_if.h"

#define DEBUG_MODE
#define MAX_LEN_DEBUG_STR 256

extern char debug_buffer[];

// Debug print function
#ifdef DEBUG_MODE
#define DEBUG_MSG(fmt, ...)                                                    \
  sprintf(debug_buffer, fmt, ##__VA_ARGS__);                                   \
  CDC_Transmit(0, (uint8_t *)debug_buffer, strlen(debug_buffer));
#else
#define DEBUG_MSG(fmt, ...)
#endif

#endif /* CORE_INC_CONFIG_H */
