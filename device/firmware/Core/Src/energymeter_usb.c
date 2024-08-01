#include <stdio.h>
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
extern uint8_t UserTxBufferFS[];

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
  "$SET-ID",
  "$SET-RTC",
  "$LOAD-LIST",
  "$LOAD-ALL",
  "$LOAD-ONE",
};

/* USB CDC response from here */
typedef enum {
  RESP_LIST_START,
  RESP_LIST_END,
  RESP_LOAD_ALL_START,
  RESP_LOAD_ALL_END,
  RESP_FILE_ENTRY,
  RESP_FILE_START,
  RESP_FILE_END,
  RESP_OK,
  RESP_ERROR,
  RESP_COUNT,
} usb_response_type_t;

const uint8_t resp[RESP_COUNT][MAX_LEN_RESP + 1] = {
  "$LIST-START",
  "$LIST-END",
  "$LOAD-ALL-START",
  "$LOAD-ALL-END",
  "$FILE-ENTRY",
  "$FILE-START",
  "$FILE-END",
  "$OK",
  "$ERROR",
};

/******************************************************************************
 * EEM usb data dump mode
 *****************************************************************************/
void mode_usb(void) {
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, GPIO_PIN_RESET);

  /* init fatfs */
  FATFS fat;

  if (f_mount(&fat, "", 1) != FR_OK) {
    BIT_SET(error_status, EEM_ERR_SD_CARD);
    HAL_GPIO_WritePin(LED_SD_ERR_GPIO_Port, LED_SD_ERR_Pin, GPIO_PIN_RESET);
  }

  uint32_t tick = HAL_GetTick();

  // we don't need to be super fast here.
  // let's just play with strings and busy wait led blink
  while (1) {
    static uint32_t cur = 0;
    cur = HAL_GetTick();

    if (tick + 500 < cur) {
      tick = cur;
      HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    }

    if (!usb_flag) {
      continue;
    }

    /**************************************************************************
     * PROTOCOL: CMD string (starts with $) +
     *           one space (ASCII 0x20), if additional parameter(s) exist +
     *           additional parameter(s) if exist
     *************************************************************************/
    if (USB_Command(CMD_SET_ID)) {
      usb_set_id(UserRxBufferFS + strlen(cmd[CMD_SET_ID]) + 1);
    }

    else if (USB_Command(CMD_SET_RTC)) {
      usb_set_rtc(UserRxBufferFS + strlen(cmd[CMD_SET_RTC]) + 1);
    }

    else if (USB_Command(CMD_LOAD_LIST)) {
      usb_load_list();
    }

    else if (USB_Command(CMD_LOAD_ALL)) {
      usb_load_all();
    }

    else if (USB_Command(CMD_LOAD_ONE)) {
      usb_load_one(UserRxBufferFS + strlen(cmd[CMD_LOAD_ONE]) + 1);
    }

    usb_flag = FALSE;
  }
}

/******************************************************************************
 * set commanded device id to flash
 * PROTOCOL: 5-byte decimal integer string(00000 ~ 65535) for a new device id
 *****************************************************************************/
void usb_set_id(uint8_t *buf) {
  uint32_t usb_ret;

  FLASH_EraseInitTypeDef erase;
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = FLASH_TARGET_PAGE;
  erase.NbPages = 1;

  uint32_t page_err = 0;

  // unlock flash
  if (HAL_FLASH_Unlock() != HAL_OK) {
    goto flash_err;
  };

  // erase target sector
  if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK) {
    goto flash_err;
  }
  
  // write canary value
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_TARGET_PAGE, FLASH_CANARY_DEVICE_ID) != HAL_OK) {
    goto flash_err;
  }

  // write device id
  *(buf + MAX_LEN_DEVICE_ID_STR) = '\0';
  uint32_t new_id = (uint32_t)atoi((const char *)buf);

  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_TARGET_PAGE + 4, new_id) != HAL_OK) {
    goto flash_err;
  }

  USB_Response(RESP_OK);
  HAL_FLASH_Lock();
  return;

flash_err:
  USB_Response(RESP_ERROR);
  HAL_FLASH_Lock();
  return;
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
void usb_load_list(void) {
  uint8_t usb_ret;
  USB_Response(RESP_LIST_START);

  FILINFO fno;
  DIR dir;

  char filename[_MAX_LFN];
  fno.lfname = filename;
  fno.lfsize = sizeof(filename);

  if (f_findfirst(&dir, &fno, "", "*.log") != FR_OK) {
    USB_Response(RESP_ERROR);
    return;
  }

  FRESULT ret;

  do {
    sprintf((char *)UserTxBufferFS, "%s %lu %s", resp[RESP_FILE_ENTRY], fno.fsize, fno.lfname);
    USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));

    ret = f_findnext(&dir, &fno);
  } while (ret == FR_OK && fno.fname[0]);

  f_closedir(&dir);

  USB_Response(RESP_LIST_END);
}

/******************************************************************************
 * load all saved files
 *****************************************************************************/
void usb_load_all(void) {
  uint8_t usb_ret;
  USB_Response(RESP_LOAD_ALL_START);

  FILINFO fno;
  DIR dir;

  char filename[_MAX_LFN];
  fno.lfname = filename;
  fno.lfsize = sizeof(filename);

  if (f_findfirst(&dir, &fno, "", "*.log") != FR_OK) {
    USB_Response(RESP_ERROR);
    return;
  }

  FRESULT ret;

  do {
    sprintf((char *)UserTxBufferFS, "%s %lu %s", resp[RESP_FILE_ENTRY], fno.fsize, fno.lfname);
    USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));

    FIL fp;

    if (f_open(&fp, fno.lfname, FA_READ) != FR_OK) {
      USB_Response(RESP_ERROR);
      return;
    };

    uint32_t total_read = 0;

    while (total_read < f_size(&fp)) {
      uint32_t read;

      if (f_read(&fp, UserTxBufferFS, APP_TX_DATA_SIZE, (UINT *)&read) != FR_OK) {
        USB_Response(RESP_ERROR);
        USB_Response(RESP_FILE_END);
        return;
      }

      total_read += read;
      USB_Transmit(UserTxBufferFS, read);
    }

    USB_Response(RESP_FILE_END);

    ret = f_findnext(&dir, &fno);
  } while (ret == FR_OK && fno.fname[0]);

  f_closedir(&dir);

  USB_Response(RESP_LOAD_ALL_END);
}

/******************************************************************************
 * load one file by its name
 * PROTOCOL: requested file name's length in decimal integer string +
 *           one space (ASCII 0x20) +
 *           requested file name
 *****************************************************************************/
void usb_load_one(uint8_t *buf) {
  uint8_t usb_ret;

  uint8_t *end = (uint8_t *)strchr((const char *)buf, ' ');
  *end = '\0';

  int namelen = atoi((const char *)buf);
  buf = end + 1;
  *(buf + namelen) = '\0';

  FIL fp;

  if (f_open(&fp, (const char *)buf, FA_READ) != FR_OK) {
    USB_Response(RESP_ERROR);
    return;
  };

  sprintf((char *)UserTxBufferFS, "%s %lu", resp[RESP_FILE_START], f_size(&fp));
  USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));

  uint32_t total_read = 0;

  while (total_read < f_size(&fp)) {
    uint32_t read;

    if (f_read(&fp, UserTxBufferFS, APP_TX_DATA_SIZE, (UINT *)&read) != FR_OK) {
      USB_Response(RESP_ERROR);
      USB_Response(RESP_FILE_END);
      return;
    }

    total_read += read;
    USB_Transmit(UserTxBufferFS, read);
  }

  USB_Response(RESP_FILE_END);
}
