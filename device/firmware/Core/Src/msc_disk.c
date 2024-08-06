/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdbool.h>
#include "bsp/board_api.h"
#include "tusb.h"
#include "user_diskio_spi.h"

#define SD_PDRV (0)
#define BLOCK_SIZE 512

uint32_t blk_cnt = 0x10000;
uint16_t blk_size = 0x200;

#if CFG_TUD_MSC

// whether host does safe-eject
static bool ejected = false;

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
  const char vid[] = "LUFT-A.";
  const char pid[] = "FSK-EEM MSC";
  const char rev[] = "1.0";

  memcpy(vendor_id, vid, strlen(vid));
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  return !USER_SPI_status(SD_PDRV);
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
  // USER_SPI_ioctl(SD_PDRV, GET_SECTOR_COUNT, block_count);
  // USER_SPI_ioctl(SD_PDRV, GET_BLOCK_SIZE, block_size);

  *block_count = blk_cnt;
  *block_size = blk_size;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
  if (load_eject) {
    if (start) {
      // load disk storage
      USER_SPI_initialize(SD_PDRV);
      USER_SPI_ioctl(SD_PDRV, GET_SECTOR_COUNT, &blk_cnt);
      USER_SPI_ioctl(SD_PDRV, GET_BLOCK_SIZE, &blk_size);
    } else {
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  if (lba >= blk_cnt) {
    return -1;
  }

  uint32_t start_sector = lba + (offset / BLOCK_SIZE);
  uint32_t sector_offset = offset % BLOCK_SIZE;
  uint32_t total_bytes_read = 0;
  uint8_t* current_buffer = (uint8_t*)buffer;
  uint32_t remaining_bytes = bufsize;

  uint8_t temp_buf[BLOCK_SIZE];

  if (sector_offset > 0) {
    if (USER_SPI_read(0, temp_buf, start_sector, 1) != RES_OK) {
      return -1;
    }
    uint32_t bytes_to_copy = (remaining_bytes > (BLOCK_SIZE - sector_offset)) ? (BLOCK_SIZE - sector_offset) : remaining_bytes;
    memcpy(current_buffer, temp_buf + sector_offset, bytes_to_copy);
    total_bytes_read += bytes_to_copy;
    remaining_bytes -= bytes_to_copy;
    current_buffer += bytes_to_copy;
    start_sector++;
  }

  uint32_t sectors_to_read = remaining_bytes / BLOCK_SIZE;
  if (sectors_to_read > 0) {
    if (USER_SPI_read(0, current_buffer, start_sector, sectors_to_read) != RES_OK) {
      return -1;
    }
    uint32_t bytes_to_copy = sectors_to_read * BLOCK_SIZE;
    total_bytes_read += bytes_to_copy;
    remaining_bytes -= bytes_to_copy;
    current_buffer += bytes_to_copy;
    start_sector += sectors_to_read;
  }

  if (remaining_bytes > 0) {
    if (USER_SPI_read(0, temp_buf, start_sector, 1) != RES_OK) {
      return -1;
    }
    memcpy(current_buffer, temp_buf, remaining_bytes);
    total_bytes_read += remaining_bytes;
  }

  return (int32_t)total_bytes_read;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  return false;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  return 0;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0]) {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
      break;
  }

  // return resplen must not larger than bufsize
  if (resplen > bufsize) {
    resplen = bufsize;
  }

  if (response && (resplen > 0)) {
    if (in_xfer) {
      memcpy(buffer, response, (size_t)resplen);
    } else {
      // SCSI output
    }
  }

  return (int32_t)resplen;
}

#endif
