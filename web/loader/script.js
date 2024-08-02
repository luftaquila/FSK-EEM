let port = undefined;
let connection = false;

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

  // TODO

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

  // TODO

  await cmd_load_info();
}

/******************************************************************************
 * Protocol $LOAD-INFO;
 *
 * load device id, disk free space and RTC time
 *
 * PROTOCOL:
 *   RESPONSE: <device id> <total sectors> <free sectors> <bytes per sector> YY-MM-DD-HH-mm-ss$OK or $ERROR
 *****************************************************************************/
async function cmd_load_info() {
  if (!await check_connection()) {
    return false;
  }

  let res = await transceive(CMD.LOAD_INFO, RESP.OK);

  if (!res) {
    return false;
  }

  // TODO
  console.log(res)

  return res;
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

  res = res.text.split('$');
  res.shift();

  if (res.at(-1) !== "OK") {
    error("데이터 손상", html_strings.data_corrupt);
    return;
  }

  res.pop(); // remove OK

  res = res.map(x => {
    return x.split(' ');
  });

  res.forEach(x => {
    let resp = x[0];
    let size = x[1];
    let name = x[2];

    if (`$${resp}` !== RESP.FILE_ENTRY || x.length !== 3) {
      error("데이터 손상", "수신한 데이터가 손상되었습니다.");
      return;
    }
  });

  console.log(res);
  // TODO
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

  // TODO

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

  // TODO

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

  let res = await transceive(CMD.DELETE_ALL, RESP.OK);

  if (!res) {
    return false;
  }

  // TODO

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

  let query = `${CMD.DELETE_ONE} ${filename.length} ${filename}`;
  let res = await transceive(query, RESP.OK);

  if (!res) {
    return false;
  }

  // TODO

}

/******************************************************************************
 * Prompt target device and open it
 *****************************************************************************/
async function check_connection() {
  if (!port || !connection) {
    try {
      port = await navigator.serial.requestPort({
        filters: [{
          usbVendorId: USB_VID,
          usbProductId: USB_PID,
        }]
      });

      await port.open({ baudRate: 9600 });
      connection = true;

      return await cmd_load_info();
    } catch (e) {
      if (!(e.name === "NotFoundError")) {
        error("장비 연결 실패", e);
      }

      return false;
    }
  } else {
    return true;
  }
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
    error("명령 전송 실패", `query: ${query}<br>error: ${e}`);
    writer.releaseLock();
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
        new Promise((_, reject) => setTimeout(reject, QUERY_TIMEOUT, new Error("timeout")))
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
    error("명령 응답 수신 실패", `query: ${query}<br>error: ${e}`);
    reader.releaseLock();
    return false;
  }

  reader.releaseLock();
  receive.time = new Date() - receive.time;
  receive.speed = (receive.bytes.length / 1024) / receive.time * 1000;
  return receive;
}

/******************************************************************************
 * Event Listeners
 *****************************************************************************/
document.addEventListener("DOMContentLoaded", (_e) => {
  if (!("serial" in navigator)) {
    error("WebSerial 미지원", html_strings.no_webserial);
    return;
  }

  document.getElementById("connect").addEventListener("click", connect);
  document.getElementById("load-list").addEventListener("click", cmd_load_list);
});

/******************************************************************************
 * Alert window
 *****************************************************************************/
function error(title, html) {
  Swal.fire({
    icon: "error",
    title: title,
    html: `<div style="line-height: 2.5rem;">${html}</div>`,
    confirmButtonText: "확인",
    customClass: { confirmButton: "btn green" },
  });
}

/************************************************************************************
 * new Date().format()
 ***********************************************************************************/
var dateFormat = function () {
  var	token = /d{1,4}|m{1,4}|yy(?:yy)?|([HhMsTt])\1?|[LloSZ]|"[^"]*"|'[^']*'/g,
    timezone = /\b(?:[PMCEA][SDP]T|(?:Pacific|Mountain|Central|Eastern|Atlantic) (?:Standard|Daylight|Prevailing) Time|(?:GMT|UTC)(?:[-+]\d{4})?)\b/g,
    timezoneClip = /[^-+\dA-Z]/g,
    pad = function (val, len) {
      val = String(val);
      len = len || 2;
      while (val.length < len) val = '0' + val;
      return val;
    };
  return function (date, mask, utc) {
    var dF = dateFormat;
    if (arguments.length == 1 && Object.prototype.toString.call(date) == '[object String]' && !/\d/.test(date)) {
      mask = date;
      date = undefined;
    }
    date = date ? new Date(date) : new Date;
    if (isNaN(date)) throw SyntaxError('invalid date');
    mask = String(dF.masks[mask] || mask || dF.masks['default']);
    if (mask.slice(0, 4) == 'UTC:') {
      mask = mask.slice(4);
      utc = true;
    }
    var	_ = utc ? 'getUTC' : 'get',
      d = date[_ + 'Date'](),
      D = date[_ + 'Day'](),
      m = date[_ + 'Month'](),
      y = date[_ + 'FullYear'](),
      H = date[_ + 'Hours'](),
      M = date[_ + 'Minutes'](),
      s = date[_ + 'Seconds'](),
      L = date[_ + 'Milliseconds'](),
      o = utc ? 0 : date.getTimezoneOffset(),
      flags = {
        d:    d,
        dd:   pad(d),
        ddd:  dF.i18n.dayNames[D],
        dddd: dF.i18n.dayNames[D + 7],
        m:    m + 1,
        mm:   pad(m + 1),
        mmm:  dF.i18n.monthNames[m],
        mmmm: dF.i18n.monthNames[m + 12],
        yy:   String(y).slice(2),
        yyyy: y,
        h:    H % 12 || 12,
        hh:   pad(H % 12 || 12),
        H:    H,
        HH:   pad(H),
        M:    M,
        MM:   pad(M),
        s:    s,
        ss:   pad(s),
        l:    pad(L, 3),
        L:    pad(L > 99 ? Math.round(L / 10) : L),
        t:    H < 12 ? 'a'  : 'p',
        tt:   H < 12 ? 'am' : 'pm',
        T:    H < 12 ? 'A'  : 'P',
        TT:   H < 12 ? '오전' : '오후',
        Z:    utc ? 'UTC' : (String(date).match(timezone) || ['']).pop().replace(timezoneClip, ''),
        o:    (o > 0 ? '-' : '+') + pad(Math.floor(Math.abs(o) / 60) * 100 + Math.abs(o) % 60, 4),
        S:    ['th', 'st', 'nd', 'rd'][d % 10 > 3 ? 0 : (d % 100 - d % 10 != 10) * d % 10]
      };
    return mask.replace(token, function ($0) {
      return $0 in flags ? flags[$0] : $0.slice(1, $0.length - 1);
    });
  };
}();
dateFormat.masks = {'default':'ddd mmm dd yyyy HH:MM:ss'};
dateFormat.i18n = {
  dayNames: [
    '일', '월', '화', '수', '목', '금', '토',
    '일요일', '월요일', '화요일', '수요일', '목요일', '금요일', '토요일'
  ],
  monthNames: [
    'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec',
    'January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'
  ]
};
Date.prototype.format = function(mask, utc) { return dateFormat(this, mask, utc); };

