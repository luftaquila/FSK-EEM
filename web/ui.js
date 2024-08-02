let port = undefined;
let connection = false;

/******************************************************************************
 * Prompt target device and open it
 *****************************************************************************/
async function check_connection() {
  if (port && connection) {
    return true;
  }

  try {
    port = await navigator.serial.requestPort({
      filters: [{
        usbVendorId: USB_VID,
        usbProductId: USB_PID,
      }]
    });

    document.getElementById("connect").classList.remove('red', 'blue');
    document.getElementById("connect").classList.add('green');
    toastr.success('장치 연결 성공');

    // disconnect event handler
    port.addEventListener("disconnect", (_e) => {
      port = undefined;
      connection = false;
      clock = undefined;

      document.getElementById("device-id").innerText = "N/A";
      document.getElementById("connect").classList.remove('green');
      document.getElementById("connect").classList.add('red');
      toastr.error(`장치 연결 해제`);
    });

    await port.open({ baudRate: 9600 });
    connection = true;

    await cmd_load_list();

    return true;
  } catch (e) {
    if (e.name === "NotFoundError") {
      toastr.warning('장치 선택 취소');
    } else {
      error("장치 연결 실패", e);
    }

    return false;
  }
}

/******************************************************************************
 * $SET-ID user prompt handler
 *****************************************************************************/
async function ui_cmd_set_id() {
  if (!await check_connection()) {
    return false;
  }

  const { isConfirmed, value } = await Swal.fire({
    title: "장치 ID 설정",
    input: "text",
    showCancelButton: true,
    confirmButtonText: "확인",
    cancelButtonText: "취소",
    customClass: { confirmButton: "btn green", cancelButton: "btn yellow" },
    preConfirm: (value) => {
      if (!value || !Number.isInteger(Number(value)) || Number(value) < 0 || Number(value) >= DEVICE_ID_BROADCAST) {
        return Swal.showValidationMessage(html_strings.id_error);
      }
    }
  });

  if (!isConfirmed) {
    return false;
  }

  cmd_set_id(Number(value));
}

/******************************************************************************
 * $LOAD-INFO response UI handler
 *****************************************************************************/
function ui_load_info(res) {
  res = res.text.replace(RESP.OK, '').split(' ');

  let id = res[0];
  let total_sector = Number(res[1]);
  let free_sector = Number(res[2]);
  let sector_size = Number(res[3]);
  let rtc = res[4];

  document.getElementById("device-id").innerText = id;

  let total = total_sector * sector_size;
  let free = free_sector * sector_size;
  let used = total - free;
  let usage = used / total * 100;

  document.getElementById("storage-free").innerText = format_byte(used);
  document.getElementById("storage-total").innerText = format_byte(total);
  document.getElementById("storage-percent").innerText = usage.toFixed(2);

  let date = rtc.substring(0, 8);
  let time = rtc.substring(9).replace(/-/g, ':');
  let century = Math.floor(new Date().getFullYear() / 100);

  clock = new Date(Date.parse(`${century}${date}T${time}`));

  document.getElementById("device-clock").innerHTML = clock.format("yyyy-mm-dd HH:MM:ss").replace(' ', '&ensp;');
}

/******************************************************************************
 * $LOAD-LIST response UI handler
 *****************************************************************************/
function ui_load_list(res) {
  res = res.text.split('$');
  res.shift();

  if (res.at(-1) !== "OK") {
    error("데이터 손상", "수신한 데이터가 손상되었습니다.");
    return;
  }

  res.pop(); // remove OK

  res = res.map(x => {
    return x.split(' ');
  });

  // clear the table
  table.data.data = [];

  // there is no file
  if (res.length === 1 && res[0][1] === '0') {
    toastr.warning('저장된 로그 파일 없음');
    return;
  }

  res.forEach(x => {
    let resp = x[0];
    let size = x[1];
    let name = x[2];

    if (`$${resp}` !== RESP.FILE_ENTRY || x.length !== 3) {
      error("데이터 손상", "수신한 데이터가 손상되었습니다.");

      // clear the table
      table.data.data = [];
      table.refresh();

      return;
    }

    table.insert({ data: [[name, format_byte(size), name]] });
  });
}

/******************************************************************************
 * $LOAD-ALL response UI handler
 *****************************************************************************/
function ui_load_all(res) {
  const resp_file_entry = Uint8Array.from(Array.from(RESP.FILE_ENTRY).map(ch => ch.charCodeAt(0)));

  let zip = new JSZip();
  let cnt = 0;

  let i = 0;

  while (i < res.bytes.length) {
    // find $FILE-ENTRY cmd
    if (res.bytes[i] === "$".charCodeAt(0) && i + resp_file_entry.length < res.bytes.length) {
      let match = true;

      for (let j = 0; j < resp_file_entry.length; j++) {
        if (res.bytes[i + j] !== resp_file_entry[j]) {
          match = false;
          break;
        }
      }

      // found $FILE-ENTRY
      if (match) {
        i += resp_file_entry.length + 1;
        let j = i + 1;

        // find next space
        while (res.bytes[j] !== " ".charCodeAt(0)) {
          j++;
        }

        // read file size
        let size = Number(String.fromCharCode(...res.bytes.slice(i, j)));

        // file name start point
        i = j + 1;
        j = i + 1;

        // find next space
        while (res.bytes[j] !== " ".charCodeAt(0)) {
          j++;
        }

        // read filename
        let filename = String.fromCharCode(...res.bytes.slice(i, j));

        // file data start point
        i = j + 1;

        let blob = new Blob([new Uint8Array(res.bytes.slice(i, i + size))], { type: "application/octet-stream" });
        zip.file(filename, blob);
        cnt++;
      }
    }

    i++;
  }

  if (cnt) {
    zip.generateAsync({ type: "blob" })
      .then(blob => {
        saveAs(blob, "log.zip");
      });
  }
}

/******************************************************************************
 * $LOAD-ONE response UI handler
 *****************************************************************************/
function ui_load_one(res, filename) {
  console.log(filename)
  const resp_file_entry = Uint8Array.from(Array.from(RESP.FILE_START).map(ch => ch.charCodeAt(0)));

  let i = 0;
  let match = true;

  for (let j = 0; j < resp_file_entry.length; j++) {
    if (res.bytes[i + j] !== resp_file_entry[j]) {
      match = false;
      break;
    }
  }

  if (!match) {
    error("데이터 손상", "수신한 데이터가 손상되었습니다.");
    return;
  }

  i += resp_file_entry.length + 1;
  let j = i + 1;

  // find next space
  while (res.bytes[j] !== " ".charCodeAt(0)) {
    j++;
  }

  // read file size
  let size = Number(String.fromCharCode(...res.bytes.slice(i, j)));

  // file data start point
  i = j + 1;

  let blob = new Blob([new Uint8Array(res.bytes.slice(i, i + size))], { type: "application/octet-stream" });
  saveAs(blob, filename);
}

/******************************************************************************
 * device clock updater
 *****************************************************************************/
let clock = undefined;

setInterval(() => {
  if (clock) {
    clock.setSeconds(clock.getSeconds() + 1);
    document.getElementById("device-clock").innerHTML = clock.format("yyyy-mm-dd HH:MM:ss").replace(' ', '&ensp;');
  }
}, 1000);

/******************************************************************************
 * Alert windows
 *****************************************************************************/
async function warn(title, html) {
  return Swal.fire({
    icon: "warning",
    title: title,
    html: `<div style="line-height: 2.5rem;">${html}</div>`,
    showCancelButton: true,
    confirmButtonText: "확인",
    cancelButtonText: "취소",
    customClass: { confirmButton: "btn red", cancelButton: "btn yellow" },
  });
}

function error(title, html) {
  Swal.fire({
    icon: "error",
    title: title,
    html: `<div style="line-height: 2.5rem;">${html}</div>`,
    confirmButtonText: "확인",
    customClass: { confirmButton: "btn blue" },
  });
}

/******************************************************************************
 * Toastr global options
 *****************************************************************************/
toastr.options = {
  closeButton: false,
  newestOnTop: false,
  // progressBar: true,
  // positionClass: "toast-bottom-right",
  debug: false,
};
