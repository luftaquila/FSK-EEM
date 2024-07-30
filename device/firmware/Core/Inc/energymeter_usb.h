#ifndef CORE_INC_ENERGYMETER_USB_H
#define CORE_INC_ENERGYMETER_USB_H

#include <stdint.h>

/******************************************************************************
 * USB commands
 *****************************************************************************/
#define MAX_LEN_CMD  10
#define MAX_LEN_RESP 10

/******************************************************************************
 * USB command / response macro
 *****************************************************************************/
#define USB_Command(CMD) \
  (strncmp((const char *)UserRxBufferFS, cmd[CMD], strlen(cmd[CMD])) == 0)

#define USB_Response(RESP) \
  USB_Transmit((uint8_t *)resp[RESP], strlen((const char *)resp[RESP]))

/******************************************************************************
 * function prototypes
 *****************************************************************************/
void mode_usb(void);

void usb_set_rtc(uint8_t *buf);
void usb_load_list(uint8_t *buf);
void usb_load_all(uint8_t *buf);
void usb_load_one(uint8_t *buf);

#endif /* CORE_INC_ENERGYMETER_USB_H */
