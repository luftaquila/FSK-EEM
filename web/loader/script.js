let port = undefined;
let connection = false;

const USB_VID = 0x1999;
const USB_PID = 0x0512;

const QUERY_TIMEOUT = 500;

/******************************************************************************
 * Protocol $LOAD-LIST
 *****************************************************************************/
async function load_list() {
  let res = await transceive(CMD.LOAD_LIST, RESP.OK);

  if (!res) {
    return;
  }

  console.log(res);
  // TODO
}

/******************************************************************************
 * Transmit query string and return response. end: stream end string
 *****************************************************************************/
async function transceive(query, end) {
  if (!port || !connection) {
    if (!await connect()) {
      return false;
    };
  }

  let reader;
  let writer;

  try {
    writer = port.writable.getWriter();
    await writer.write(Uint8Array.from(Array.from(query).map(ch => ch.charCodeAt(0))));
    writer.releaseLock();
  } catch (e) {
    Swal.fire({
      icon: "error",
      title: "명령 전송 실패",
      html: e,
      confirmButtonText: "확인",
      customClass: { confirmButton: "btn green" },
    });
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
    Swal.fire({
      icon: "error",
      title: "데이터 수신 실패",
      html: e,
      confirmButtonText: "확인",
      customClass: { confirmButton: "btn green" },
    });
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
  } catch (e) {
    if (!(e.name === "NotFoundError")) {
      Swal.fire({
        icon: "error",
        title: "장비 연결 실패",
        html: e,
        confirmButtonText: "확인",
        customClass: { confirmButton: "btn green" },
      });
    }

    return false;
  }

  return true;
}


/******************************************************************************
 * Event Listeners
 *****************************************************************************/
document.addEventListener("DOMContentLoaded", (_e) => {
  if (!("serial" in navigator)) {
    Swal.fire({
      icon: "error",
      title: "WebSerial 미지원",
      html: html_strings.no_webserial,
      confirmButtonText: "확인",
      customClass: { confirmButton: "btn green" },
    });

    return;
  }

  document.getElementById("connect").addEventListener("click", connect);
  document.getElementById("load-list").addEventListener("click", load_list);
});

