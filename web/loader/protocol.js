/******************************************************************************
 * USB command and response protocol
 * defined in device/firmware/Core/Src/energymeter_usb.c
 *****************************************************************************/
const CMD = {
  SET_ID: "$SET-ID",
  SET_RTC: "$SET-RTC",
  LOAD_INFO: "$LOAD-INFO",
  LOAD_LIST: "$LOAD-LIST",
  LOAD_ALL: "$LOAD-ALL",
  LOAD_ONE: "$LOAD-ONE",
};

const RESP = {
  FILE_ENTRY: "$FILE-ENTRY",
  FILE_START: "$FILE-START",
  FILE_END: "$FILE-END",
  OK: "$OK",
  ERROR: "$ERROR",
}

