#include <stdint.h>
#include <string.h>

#include "ff.h"
#include "usbd_cdc_if.h"

#include "energymeter.h"
#include "energymeter_usb.h"

/******************************************************************************
 * global variables
 *****************************************************************************/
/* USB CDC receive flag and buffer */
uint32_t usb_flag = FALSE;
extern uint8_t UserRxBufferFS[];

/* USB CDC command from the host system */
typedef enum {
  CMD_SET_ID,
  CMD_SET_RTC,
  CMD_LOAD_LIST,
  CMD_LOAD_ALL,
  CMD_LOAD_ONE,
  CMD_COUNT,
} usb_cmd_type_t;

const char cmd[CMD_COUNT][MAX_LEN_CMD + 1] = {
  "SET-ID",
  "SET-RTC",
  "LOAD-LIST",
  "LOAD-ALL",
  "LOAD-ONE",
};

/* USB CDC response from here */
typedef enum {
  RESP_LIST_START,
  RESP_LIST_END,
  RESP_COUNT,
} usb_response_type_t;

const uint8_t resp[RESP_COUNT][MAX_LEN_RESP + 1] = {
  "LIST-START",
  "LIST-END",
};

/******************************************************************************
 * EEM usb data dump mode
 *****************************************************************************/
void mode_usb(void) {
  /* read device id from the flash */
  device_id_t *devid = (device_id_t *)FLASH_TARGET_PAGE;

  /* init fatfs */
  FATFS fat;

  if (f_mount(&fat, "", 1) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
    HAL_GPIO_WritePin(LED_SD_ERR_GPIO_Port, LED_SD_ERR_Pin, GPIO_PIN_RESET);
  }

  uint32_t tick = HAL_GetTick();

  // we don't need to be super fast here.
  // let's just compare strings and busy wait led blink
  while (TRUE) {
    static uint32_t cur = 0;
    cur = HAL_GetTick();

    if (tick + 1000 > cur) {
      tick = cur;
      HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    }

    if (!usb_flag) {
      continue;
    }

    if (USB_Command(CMD_SET_ID)) {
      // TODO
    }

    else if (USB_Command(CMD_SET_RTC)) {
      usb_set_rtc(UserRxBufferFS + strlen(cmd[CMD_SET_RTC]));
    }

    else if (USB_Command(CMD_LOAD_LIST)) {
      usb_load_list(UserRxBufferFS + strlen(cmd[CMD_LOAD_LIST]));
    }

    else if (USB_Command(CMD_LOAD_ALL)) {
      usb_load_all(UserRxBufferFS + strlen(cmd[CMD_LOAD_ALL]));
    }

    else if (USB_Command(CMD_LOAD_ONE)) {
      usb_load_one(UserRxBufferFS + strlen(cmd[CMD_LOAD_ONE]));
    }
  }
}

/******************************************************************************
 * set commanded RTC time
 *****************************************************************************/
void usb_set_rtc(uint8_t *buf) {
  // TODO
}

/******************************************************************************
 * load all saved files list
 *****************************************************************************/
void usb_load_list(uint8_t *buf) {
  uint8_t usb_ret;
  USB_Response(RESP_LIST_START);

  FRESULT ret;
  FILINFO fno;
  DIR dir;

  char filename[_MAX_LFN];
  fno.lfname = filename;
  fno.lfsize = sizeof(filename);

  while (TRUE) {
    ret = f_findfirst(&dir, &fno, "", "*.log");

    while (ret == FR_OK && fno.fname[0]) {
      // TODO

      ret = f_findnext(&dir, &fno);
    }

    f_closedir(&dir);
  }
}

/******************************************************************************
 * load all saved files
 *****************************************************************************/
void usb_load_all(uint8_t *buf) {
  // TODO
}

/******************************************************************************
 * load one file by its name
 *****************************************************************************/
void usb_load_one(uint8_t *buf) {
  // TODO
}
