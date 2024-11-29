const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const colorInput = document.getElementById("color");
const colorPreview = document.getElementById("color-preview");

// 캔버스 크기를 확대하여 표시 (픽셀 확대 효과)
canvas.style.width = "500px";
canvas.style.height = "500px";

// 색상(hex)을 RGB 객체로 변환하는 함수
function hexToRGB(hex) {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return { r, g, b };
}

// 초기 색상 미리보기 설정
updateColorPreview();

// 색상 변경 시 미리보기 업데이트
colorInput.addEventListener("input", updateColorPreview);

// 캔버스 초기화
let pixelData = new Uint8Array(100 * 100 * 4); // RGBA 형식

// 캔버스 초기화: 모든 픽셀을 흰색으로 설정
for (let i = 0; i < pixelData.length; i += 4) {
  pixelData[i] = 255; // R
  pixelData[i + 1] = 255; // G
  pixelData[i + 2] = 255; // B
  pixelData[i + 3] = 255; // A (불투명도)
}
updateCanvas();

// 서버에 연결
const ws = new WebSocket("ws://localhost:17171"); // C 서버의 포트로 연결
ws.binaryType = "arraybuffer";

ws.onopen = function () {
  console.log("WebSocket 연결 성공");
};
ws.onmessage = function (event) {

  const data = new Uint8Array(event.data);
  let i = 0;
  while (i + 2 < data.length) {
    const x = data[i];
    const y = data[i + 1];
    const color = data[i + 2];
    i += 3;

    // 색상을 적용하여 픽셀 데이터 업데이트
    const index = (y * 100 + x) * 4;
    const rgbColor = colorToRGB(color);
    pixelData[index] = rgbColor.r;
    pixelData[index + 1] = rgbColor.g;
    pixelData[index + 2] = rgbColor.b;
    pixelData[index + 3] = 255; // 불투명도 (변경하지 않음)
  }
  updateCanvas();
};

ws.onclose = function () {
  console.log("WebSocket 연결 종료");
};

ws.onerror = function (error) {
  console.error("WebSocket 오류:", error);
};

// 캔버스에 클릭 이벤트 리스너 추가
canvas.addEventListener("click", function (event) {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  const x = Math.floor((event.clientX - rect.left) * scaleX);
  const y = Math.floor((event.clientY - rect.top) * scaleY);

  const rgb = hexToRGB(colorInput.value);
  const message = new Uint8Array([x, y, rgb.r, rgb.g, rgb.b]);
  ws.send(message.buffer);
  if (ws.readyState === WebSocket.OPEN) {
    console.log(`Sent pixel update: (${x}, ${y}) with color ${colorInput.value}`);
  } else {
    console.warn("WebSocket이 열려 있지 않습니다. 메시지를 보낼 수 없습니다.");
  }
});

// 캔버스 업데이트 함수
function updateCanvas() {
  const imageData = new ImageData(new Uint8ClampedArray(pixelData), 100, 100);
  ctx.putImageData(imageData, 0, 0);
}
