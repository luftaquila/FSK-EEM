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

const DEVICE_ID_INVALID = 0xFFFF;
const DEVICE_ID_BROADCAST = 0xFFFE;

const QUERY_TIMEOUT = 500;
const QUERY_TIMEOUT_LONG = 5 * 60 * 1000; // 5 min

/******************************************************************************
 * Protocol $SET-ID
 *
 * set commanded device id to flash
 *
 * PROTOCOL:
 *      QUERY: 5-byte decimal integer string(00000 ~ 65535) for a new device id
 *   RESPONSE: $OK or $ERROR
 *****************************************************************************/
async function cmd_set_id(id) {
  if (!await check_connection()) {
    return false;
  }

  let query = `${CMD.SET_ID} ${String(id).padStart(5, '0')}`
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

  let query = `${CMD.SET_RTC} ${new Date().format('yy-mm-dd-hh-MM-ss')}`;
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

  await cmd_load_info();

  let res = await transceive(CMD.LOAD_LIST, RESP.OK, 5000);

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

  ui_info_download(files.total);

  let res = await transceive(CMD.LOAD_ALL, RESP.OK, QUERY_TIMEOUT_LONG);

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

  ui_info_download(files.list.find(x => x.name === filename).size);

  let query = `${CMD.LOAD_ONE} ${filename.length} ${filename}`;
  let res = await transceive(query, RESP.FILE_END, QUERY_TIMEOUT_LONG);

  if (!res) {
    return false;
  }

  toastr.success('파일 데이터 수신 완료');
  ui_load_one(res, filename);
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
async function transceive(query, end, timeout = QUERY_TIMEOUT) {
  let reader;
  let writer;

  console.log(query);

  try {
    writer = port.writable.getWriter();
    await writer.write(Uint8Array.from(Array.from(query).map(ch => ch.charCodeAt(0))));
    writer.releaseLock();
  } catch (e) {
    writer.releaseLock();
    error("명령 전송 실패", `cmd: ${query}<br>error: ${e}`);
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
        new Promise((_, reject) => setTimeout(reject, timeout, new Error("Response Timeout")))
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
    error("명령 응답 수신 실패", `cmd: ${query}<br>error: ${e}<br>response: ${receive.text}`);
    return false;
  }

  receive.time = new Date() - receive.time;
  receive.speed = receive.bytes.length / receive.time * 1000;

  reader.releaseLock();

  if (receive.text.includes(RESP.ERROR)) {
    error("장치 오류", `cmd: ${query}<br>response: ${receive.text}`);
    return false;
  }

  console.log(receive.text);

  document.getElementById("bytes-downloaded").innerText = format_byte(receive.bytes.length);
  document.getElementById("link-speed").innerText = format_byte(receive.speed);
  document.getElementById("time-spent").innerText = (receive.time / 1000).toFixed(1);

  return receive;
}
