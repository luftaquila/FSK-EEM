#ifndef CORE_INC_ENERGYMETER_H
#define CORE_INC_ENERGYMETER_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/******************************************************************************
 * debug serial configuration
 */
#define DEBUG_MODE 1
#define MAX_LEN_DEBUG_STR 256

extern char debug_buffer[];

// Debug print function
#if DEBUG_MODE
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
#define UART_CONS  huart1
#define SPI_SD     hspi1
#define SPI_RF     hspi2


/******************************************************************************
 * function prototypes
 */
void mode_energymeter(void);
void mode_usb(void);

#endif /* CORE_INC_ENERGYMETER_H */
