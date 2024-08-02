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

    toastr.success('장치 연결 성공');

    // disconnect event handler
    port.addEventListener("disconnect", (_e) => {
      port = undefined;
      connection = false;

      toastr.error(`장치 연결 해제`);
    });

    await port.open({ baudRate: 9600 });
    connection = true;

    await cmd_load_info();

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
 * $LOAD-INFO response UI handler
 *****************************************************************************/
function ui_load_info(res) {
  // TODO
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

  // TODO
}

/******************************************************************************
 * $LOAD-ALL response UI handler
 *****************************************************************************/
function ui_load_all(res) {
  // TODO
}

/******************************************************************************
 * $LOAD-ONE response UI handler
 *****************************************************************************/
function ui_load_one(res) {
  // TODO
}

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
