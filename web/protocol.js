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
  DELETE_ALL: "$DELETE-ALL",
  DELETE_ONE: "$DELETE-ONE",
};

const RESP = {
  FILE_ENTRY: "$FILE-ENTRY",
  FILE_START: "$FILE-START",
  FILE_END: "$FILE-END",
  OK: "$OK",
  ERROR: "$ERROR",
}

const USB_VID = 0x1999;
const USB_PID = 0x0512;

const QUERY_TIMEOUT = 500;

/******************************************************************************
 * Protocol $SET-ID
 *
 * set commanded device id to flash
 *
 * PROTOCOL:
 *      QUERY: 5-byte decimal integer string(00000 ~ 65535) for a new device id
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
async function cmd_set_id(new_id) {
  if (!await check_connection()) {
    return false;
  }

  let query = `${CMD.SET_ID} ${String(new_id).padStart(5, '0')}`
  let res = await transceive(query, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('장치 ID 설정 완료');
  await cmd_load_info();
}

/******************************************************************************
 * Protocol $SET-RTC
 *
 * set commanded RTC time
 *
 * PROTOCOL:
 *      QUERY: YY-MM-DD-HH-mm-ss
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
async function cmd_set_rtc() {
  if (!await check_connection()) {
    return false;
  }

  let query = `${CMD.SET_RTC} ${new Date().format('yyyy-mm-dd-hh-MM-ss')}`;
  let res = await transceive(query, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('장치 시간 설정 완료');
  await cmd_load_info();
}

/******************************************************************************
 * Protocol $LOAD-INFO;
 *
 * load device id, disk free space and RTC time
 *
 * PROTOCOL:
 *   RESPONSE: <device id> <total sectors> <free sectors> <sector size> YY-MM-DD-HH-mm-ss$OK or $ERROR
 *****************************************************************************/
async function cmd_load_info() {
  if (!await check_connection()) {
    return false;
  }

  let res = await transceive(CMD.LOAD_INFO, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('장치 정보 수신 완료');
  ui_load_info(res);
}

/******************************************************************************
 * Protocol $LOAD-LIST
 *
 * load all saved files list
 *
 * PROTOCOL:
 *   RESPONSE: [$FILE-ENTRY <size> <name>] * n + $OK or $ERROR
 *****************************************************************************/
async function cmd_load_list() {
  if (!await check_connection()) {
    return false;
  }

  let res = await transceive(CMD.LOAD_LIST, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('파일 목록 수신 완료');
  ui_load_list(res);
}

/******************************************************************************
 * Protocol $LOAD-ALL
 *
 * load all saved files
 *
 * PROTOCOL:
 *   RESPONSE: [$FILE-ENTRY <size> <name> <content>$FILE-END] * n + $OK or $ERROR
 *****************************************************************************/
async function cmd_load_all() {
  if (!await check_connection()) {
    return false;
  }

  let res = await transceive(CMD.LOAD_ALL, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('전체 파일 데이터 수신 완료');
  ui_load_all(res);
}

/******************************************************************************
 * Protocol $LOAD-ONE
 *
 * load one file by its name
 *
 * PROTOCOL:
 *      QUERY: <filename length in decimal integer string> <filename>
 *   RESPONSE: $FILE-START <size> <content>$FILE-END or $ERROR
 *****************************************************************************/
async function cmd_load_one(filename) {
  if (!await check_connection()) {
    return false;
  }

  let query = `${CMD.LOAD_ONE} ${filename.length} ${filename}`;
  let res = await transceive(query, RESP.FILE_END);

  if (!res) {
    return false;
  }

  toastr.success('파일 데이터 수신 완료');
  ui_load_one(res);
}

/******************************************************************************
 * Protocol $DELETE-ALL
 *
 * DELETE all saved files
 *
 * PROTOCOL:
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
async function cmd_delete_all() {
  if (!await check_connection()) {
    return false;
  }

  const { isConfirmed } = await warn('로그 전체 삭제', '이 작업은 되돌릴 수 없습니다.');

  if (!isConfirmed) {
    return false;
  }

  let res = await transceive(CMD.DELETE_ALL, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('로그 전체 삭제 완료');
  await cmd_load_list();
}

/******************************************************************************
 * Protocol $DELETE-ONE
 *
 * DELETE one file by its name
 *
 * PROTOCOL:
 *      QUERY: <filename length in decimal integer string> <filename>
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
async function cmd_delete_one(filename) {
  if (!await check_connection()) {
    return false;
  }

  const { isConfirmed } = await warn('로그 삭제', `파일: ${filename}<br>이 작업은 되돌릴 수 없습니다.`);

  if (!isConfirmed) {
    return false;
  }

  let query = `${CMD.DELETE_ONE} ${filename.length} ${filename}`;
  let res = await transceive(query, RESP.OK);

  if (!res) {
    return false;
  }

  toastr.success('로그 삭제 완료');
  await cmd_load_list();
}

/******************************************************************************
 * Transmit query string and return response. end: stream end string
 *****************************************************************************/
async function transceive(query, end) {
  let reader;
  let writer;

  try {
    writer = port.writable.getWriter();
    await writer.write(Uint8Array.from(Array.from(query).map(ch => ch.charCodeAt(0))));
    writer.releaseLock();
  } catch (e) {
    writer.releaseLock();
    error("명령 전송 실패", `query: ${query}<br>error: ${e}`);
    return false;
  }

  reader = port.readable.getReader();

  let receive = {
    bytes: [],
    text: "",
    time: new Date(),
    speed: 0,
  };

  try {
    while (port && port.readable) {
      const { value, done } = await Promise.race([
        reader.read(),
        new Promise((_, reject) => setTimeout(reject, QUERY_TIMEOUT, new Error("Command Response Timeout")))
      ]);

      if (done) {
        break;
      }

      if (value) {
        receive.text += new TextDecoder().decode(value);
        receive.bytes = [...receive.bytes, ...Array.from(value)];

        if (receive.text.includes(end) || receive.text.includes(RESP.ERROR)) {
          break;
        }
      }
    }
  } catch (e) {
    reader.releaseLock();
    error("명령 응답 수신 실패", `query: ${query}<br>error: ${e}`);
    return false;
  }

  reader.releaseLock();
  receive.time = new Date() - receive.time;
  receive.speed = (receive.bytes.length / 1024) / receive.time * 1000;
  return receive;
}
