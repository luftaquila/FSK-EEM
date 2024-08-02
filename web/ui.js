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

    document.getElementById("connection").style.color = "green";
    toastr.success('장치 연결 성공');

    // disconnect event handler
    port.addEventListener("disconnect", (_e) => {
      port = undefined;
      connection = false;
      clock = undefined;

      document.getElementById("device-id").innerText = "N/A";
      document.getElementById("connection").style.color = "red";
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
 * $LOAD-ALL response UI handler
 *****************************************************************************/
function ui_load_all(res) {
  console.log(res);
  // TODO
}

/******************************************************************************
 * $LOAD-ONE response UI handler
 *****************************************************************************/
function ui_load_one(res) {
  console.log(res);
  // TODO
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
  positionClass: "toast-bottom-right",
  debug: false,
};
