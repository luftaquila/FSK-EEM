#ifndef CORE_INC_TYPES_H_
#define CORE_INC_TYPES_H_

#define DEBUG_MODE

// Debug print function
#ifdef DEBUG_MODE
#define DEBUG_MSG(fmt, ...) printf("[%8lu] " fmt "\r\n", HAL_GetTick(), ##__VA_ARGS__)
#else
#define DEBUG_MSG(fmt, ...)
#endif

// Peripheral definitions
#define UART_DEBUG huart1
#define SPI_SD     hspi1
#define SPI_RF     hspi2

#endif /* CORE_INC_TYPES_H_ */
