let port = undefined;
let connection = false;

const USB_VID = 0x1999;
const USB_PID = 0x0512;

const QUERY_TIMEOUT = 500;

/******************************************************************************
 * Protocol $LOAD-LIST
 *****************************************************************************/
async function load_list() {
  if (!port || !connection) {
    let info = await connect();

    if (!info) {
      return false;
    } else {
      set_info(info);
    };
  }

  let res = await transceive(CMD.LOAD_LIST, RESP.OK);

  if (!res) {
    return;
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
      error("데이터 손상", html_strings.data_corrupt);
      return;
    }
  });

  console.log(res);
  // TODO
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
    error("명령 전송 실패", e);
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
    error("데이터 수신 실패", e);
    reader.releaseLock();
    return false;
  }

  reader.releaseLock();
  receive.time = new Date() - receive.time;
  receive.speed = (receive.bytes.length / 1024) / receive.time * 1000;
  return receive;
}

/******************************************************************************
 * Prompt target device and open it
 *****************************************************************************/
async function connect() {
  try {
    port = await navigator.serial.requestPort({
      filters: [{
        usbVendorId: USB_VID,
        usbProductId: USB_PID,
      }]
    });

    await port.open({ baudRate: 9600 });
    connection = true;

    return await transceive(CMD.LOAD_INFO, RESP.OK);
  } catch (e) {
    if (!(e.name === "NotFoundError")) {
      error("장비 연결 실패", e);
    }

    return false;
  }
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
  document.getElementById("load-list").addEventListener("click", load_list);
});

/******************************************************************************
 * Alert window
 *****************************************************************************/
function set_info(info) {
  console.log(info);
  // TODO
}

/******************************************************************************
 * Alert window
 *****************************************************************************/
function error(title, html) {
  Swal.fire({
    icon: "error",
    title: title,
    html: html,
    confirmButtonText: "확인",
    customClass: { confirmButton: "btn green" },
  });
}
