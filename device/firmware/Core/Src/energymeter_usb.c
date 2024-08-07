#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ff.h"
#include "tusb.h"

#include "energymeter.h"
#include "energymeter_usb.h"

/******************************************************************************
 * global variables
 *****************************************************************************/
/* USB CDC receive flag and buffer */
uint32_t usb_flag = FALSE;

/* USB CDC command from the host system */
typedef enum {
  CMD_SET_ID,
  CMD_SET_RTC,
  CMD_LOAD_INFO,
  CMD_LOAD_LIST,
  CMD_LOAD_ALL,
  CMD_LOAD_ONE,
  CMD_DELETE_ALL,
  CMD_DELETE_ONE,
  CMD_COUNT,
} usb_cmd_type_t;

const char cmd[CMD_COUNT][MAX_LEN_CMD + 1] = {
  "$SET-ID",
  "$SET-RTC",
  "$LOAD-INFO",
  "$LOAD-LIST",
  "$LOAD-ALL",
  "$LOAD-ONE",
  "$DELETE-ALL",
  "$DELETE-ONE",
};

/* USB CDC response from here */
typedef enum {
  RESP_FILE_ENTRY,
  RESP_FILE_START,
  RESP_FILE_END,
  RESP_OK,
  RESP_ERROR,
  RESP_COUNT,
} usb_response_type_t;

const uint8_t resp[RESP_COUNT][MAX_LEN_RESP + 1] = {
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

  // it is not important that this failed or not
  (void)f_setlabel(VOLUME_LABEL);

  uint32_t tick = HAL_GetTick();
  HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);

  // TinyUSB init
  tusb_init();

  // we don't need to be super fast here.
  // let's just play with strings and busy wait led blink
  while (1) {
    tud_task();

    // blink led
    static uint32_t cur = 0;
    static uint32_t blink_fast = FALSE;

    cur = HAL_GetTick();

    if ((blink_fast && (tick + 100 < cur)) || (!blink_fast && (tick + 500 < cur))) {
      tick = cur;
      blink_fast = !blink_fast;
      HAL_GPIO_TogglePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin);
    }

    if (!usb_flag) {
      continue;
    }

#ifdef DISABLED
    /**************************************************************************
     * PROTOCOL: CMD string (starts with $) +
     *           one space (ASCII 0x20), if additional parameter(s) exist +
     *           additional parameter(s) if exist
     *************************************************************************/
    if (USB_COMMAND(CMD_SET_ID)) {
      usb_set_id(UserRxBufferFS + strlen(cmd[CMD_SET_ID]) + 1);
    }

    else if (USB_COMMAND(CMD_SET_RTC)) {
      usb_set_rtc(UserRxBufferFS + strlen(cmd[CMD_SET_RTC]) + 1);
    }

    else if (USB_COMMAND(CMD_LOAD_INFO)) {
      usb_load_info();
    }

    else if (USB_COMMAND(CMD_LOAD_LIST)) {
      usb_load_list();
    }

    else if (USB_COMMAND(CMD_LOAD_ALL)) {
      usb_load_all();
    }

    else if (USB_COMMAND(CMD_LOAD_ONE)) {
      usb_load_one(UserRxBufferFS + strlen(cmd[CMD_LOAD_ONE]) + 1);
    }

    else if (USB_COMMAND(CMD_DELETE_ALL)) {
      usb_delete_all();
    }

    else if (USB_COMMAND(CMD_DELETE_ONE)) {
      usb_delete_one(UserRxBufferFS + strlen(cmd[CMD_DELETE_ONE]) + 1);
    }
#endif

    usb_flag = FALSE;
  }
}

#ifdef DISABLED
/******************************************************************************
 * set commanded device id to flash
 * PROTOCOL:
 *      QUERY: 5-byte decimal integer string(00000 ~ 65535) for a new device id
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
void usb_set_id(uint8_t *buf) {
  uint8_t usb_ret;

  FLASH_EraseInitTypeDef erase;
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = FLASH_TARGET_PAGE;
  erase.NbPages = 1;

  uint32_t page_err = 0;

  // unlock flash
  if (HAL_FLASH_Unlock() != HAL_OK) {
    // do fail only if flash is yet locked.
    //   on the second call , HAL_FLASH_Unlock() fails and the flash is already unlocked.
    //   suspect HAL_FLASH_Lock() is not working
    if (FLASH->CR & FLASH_CR_LOCK) {
      goto flash_err;
    }
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

  HAL_FLASH_Lock();
  USB_RESPONSE(RESP_OK);
  return;

flash_err:
  HAL_FLASH_Lock();
  USB_RESPONSE(RESP_ERROR);
  return;
}

/******************************************************************************
 * set commanded RTC time
 * PROTOCOL:
 *      QUERY: YY-MM-DD-HH-mm-ss
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
void usb_set_rtc(uint8_t *buf) {
  uint8_t usb_ret;

  if (rtc_set(buf) != HAL_OK) {
    USB_RESPONSE(RESP_ERROR);
    return;
  };

  USB_RESPONSE(RESP_OK);
}

/******************************************************************************
 * load device id, disk free space and RTC time
 * PROTOCOL:
 *   RESPONSE: <device id> <total sectors> <free sectors> <sector size> YY-MM-DD-HH-mm-ss$OK or $ERROR
 *****************************************************************************/
void usb_load_info(void) {
  uint8_t usb_ret;

  /* read device id from the flash */
  device_id_t *devid = (device_id_t *)FLASH_TARGET_PAGE;

  /* load disk space info */
  FATFS *fs;
  uint32_t free;

  if (f_getfree("", (DWORD *)&free, &fs) != FR_OK) {
    USB_RESPONSE(RESP_ERROR);
    return;
  }

  uint32_t sector_total = (fs->n_fatent - 2) * fs->csize;
  uint32_t sector_free = free * fs->csize;

  datetime cur;
  (void)rtc_read(&cur); // ignore failure; will be shown at the host

  sprintf((char *)UserTxBufferFS, "%05lu %lu %lu %u %02d-%02d-%02d-%02d-%02d-%02d",
          devid->id, sector_total, sector_free, _MAX_SS,
          cur.year, cur.month, cur.day, cur.hour, cur.minute, cur.second);

  USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));
  USB_RESPONSE(RESP_OK);
}

/******************************************************************************
 * load all saved files list
 * PROTOCOL:
 *   RESPONSE: [$FILE-ENTRY <size> <name>] * n + $OK or $ERROR
 *****************************************************************************/
void usb_load_list(void) {
  uint8_t usb_ret;

  FILINFO fno;
  DIR dir;

  char filename[_MAX_LFN];
  fno.lfname = filename;
  fno.lfsize = sizeof(filename);

  if (f_findfirst(&dir, &fno, "", "*.log") != FR_OK) {
    USB_RESPONSE(RESP_ERROR);
    return;
  }

  FRESULT ret;

  do {
    sprintf((char *)UserTxBufferFS, "%s %lu %s", resp[RESP_FILE_ENTRY], fno.fsize, fno.lfname);
    USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));

    ret = f_findnext(&dir, &fno);
  } while (ret == FR_OK && fno.fname[0]);

  f_closedir(&dir);

  USB_RESPONSE(RESP_OK);
}

/******************************************************************************
 * load all saved files
 * PROTOCOL:
 *   RESPONSE: [$FILE-ENTRY <size> <name> <content>$FILE-END] * n + $OK or $ERROR
 *****************************************************************************/
void usb_load_all(void) {
  uint8_t usb_ret;

  FILINFO fno;
  DIR dir;

  char filename[_MAX_LFN];
  fno.lfname = filename;
  fno.lfsize = sizeof(filename);

  if (f_findfirst(&dir, &fno, "", "*.log") != FR_OK) {
    USB_RESPONSE(RESP_ERROR);
    return;
  }

  FRESULT ret;
  uint32_t use_primary = TRUE;

  do {
    sprintf((char *)UserTxBufferFS, "%s %lu %s ", resp[RESP_FILE_ENTRY], fno.fsize, fno.lfname);
    USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));

    FIL fp;

    if (f_open(&fp, fno.lfname, FA_READ) != FR_OK) {
      USB_RESPONSE(RESP_ERROR);
      return;
    };

    uint32_t total_read = 0;

    while (total_read < f_size(&fp)) {
      uint32_t read;

      // switch buffers to speed up
      uint8_t *buf = use_primary ? UserTxBufferFS : UserTxBufferFS_2;

      if (f_read(&fp, buf, APP_TX_DATA_SIZE, (UINT *)&read) != FR_OK) {
        USB_RESPONSE(RESP_ERROR);
        return;
      }

      total_read += read;
      USB_Transmit(buf, read);

      use_primary = !use_primary;
    }

    USB_RESPONSE(RESP_FILE_END);

    ret = f_findnext(&dir, &fno);
  } while (ret == FR_OK && fno.fname[0]);

  f_closedir(&dir);

  USB_RESPONSE(RESP_OK);
}

/******************************************************************************
 * load one file by its name
 * PROTOCOL:
 *      QUERY: <filename length in decimal integer string> <filename>
 *   RESPONSE: $FILE-START <size> <content>$FILE-END or $ERROR
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
    USB_RESPONSE(RESP_ERROR);
    return;
  };

  sprintf((char *)UserTxBufferFS, "%s %lu ", resp[RESP_FILE_START], f_size(&fp));
  USB_Transmit(UserTxBufferFS, strlen((const char *)UserTxBufferFS));

  uint32_t total_read = 0;
  uint32_t use_primary = TRUE;


  // it took ~700 ms to send first 100 KB.  140 KB/s
  // it took ~1900 ms to send next 100 KB.  55 KB/s
  // it took ~3300 ms to send third 100 KB. 30 KB/s
  while (total_read < f_size(&fp)) {
    uint32_t read;

    // switch buffers to speed up
    uint8_t *buf = use_primary ? UserTxBufferFS : UserTxBufferFS_2;

    // f_read took typically 6 ms
    if (f_read(&fp, buf, APP_TX_DATA_SIZE, (UINT *)&read) != FR_OK) {
      USB_RESPONSE(RESP_ERROR);
      return;
    }

    total_read += read;

    // USB_Transmit normally took 0 ms.
    // but after first 100 KB sent, once at a fifth, it took avg 130 ms.
    // so, in avg of total, it took 25 ms
    USB_Transmit(buf, read);
    use_primary = !use_primary;
  }

  USB_RESPONSE(RESP_FILE_END);
}

/******************************************************************************
 * DELETE all saved files
 * PROTOCOL:
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
void usb_delete_all(void) {
  uint8_t usb_ret;

  FILINFO fno;
  DIR dir;

  char filename[_MAX_LFN];
  fno.lfname = filename;
  fno.lfsize = sizeof(filename);

  if (f_findfirst(&dir, &fno, "", "*.log") != FR_OK) {
    USB_RESPONSE(RESP_ERROR);
    return;
  }

  FRESULT ret;

  do {
    if (f_unlink(fno.lfname) != FR_OK) {
      USB_RESPONSE(RESP_ERROR);
      return;
    }

    ret = f_findnext(&dir, &fno);
  } while (ret == FR_OK && fno.fname[0]);

  USB_RESPONSE(RESP_OK);
}

/******************************************************************************
 * DELETE one file by its name
 * PROTOCOL:
 *      QUERY: <filename length in decimal integer string> <filename>
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
void usb_delete_one(uint8_t *buf) {
  uint8_t usb_ret;

  uint8_t *end = (uint8_t *)strchr((const char *)buf, ' ');
  *end = '\0';

  int namelen = atoi((const char *)buf);
  buf = end + 1;
  *(buf + namelen) = '\0';

  if (f_unlink((const char *)buf) != FR_OK) {
    USB_RESPONSE(RESP_ERROR);
    return;
  };

  USB_RESPONSE(RESP_OK);
}
#endif
