const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");
const ws = new WebSocket("ws://localhost:17171"); // 서버 주소
const colorPalette = document.getElementById("color-palette");

// 캔버스 크기 확대
canvas.style.width = "500px";
canvas.style.height = "500px";

// 기본 색상 목록 (40가지 색상)
const colors = ['#ff0000', '#ffa600', '#ffe600', '#8aff00', '#24ff00', '#00ff00', '#00ff83', '#00ffe6', '#00e7ff', '#008fff', '#0024ff', '#2400ff', '#8a00ff', '#ff00ff', '#ff0083', '#ff0024', '#d70000', '#8c4000', '#e67c00', '#b1d700', '#4cc100',
  '#00c16a', '#00c1c1', '#0055c1', '#002fc1', '#7200c1', '#c100c1', '#c10072', '#8c2600',
  '#4c0f00'];

// 색상 팔레트 생성
colors.forEach(color => {
  const colorBox = document.createElement("div");
  colorBox.className = "color-box";
  colorBox.style.backgroundColor = color;
  colorBox.addEventListener("click", () => {
    // 선택된 컬러로 처리
    selectColor(color);

    // 이전 활성화된 컬러 박스에서 active 클래스 제거
    document.querySelectorAll('.color-box.active').forEach(activeBox => {
      activeBox.classList.remove('active');
    });

    // 현재 클릭된 컬러 박스에 active 클래스 추가
    colorBox.classList.add('active');
  });
  colorPalette.appendChild(colorBox);
});

// 선택된 색상 저장
let selectedColor = "#000000";

function selectColor(color) {
  selectedColor = color;
  console.log(`선택된 색상: ${color}`);
}

// 캔버스 클릭 이벤트
canvas.addEventListener("click", (event) => {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  const x = Math.floor((event.clientX - rect.left) * scaleX);
  const y = Math.floor((event.clientY - rect.top) * scaleY);

  const rgb = hexToRGB(selectedColor);
  const message = new Uint8Array([x, y, rgb.r, rgb.g, rgb.b]);
  ws.send(message.buffer);
  console.log(`좌표 (${x}, ${y})로 색상 전송: ${selectedColor}`);
});

// WebSocket 처리
ws.onopen = () => console.log("WebSocket 연결 성공");
ws.onmessage = (event) => updateCanvas(new Uint8Array(event.data));
ws.onerror = (error) => console.error("WebSocket 오류:", error);
ws.onclose = () => console.log("WebSocket 연결 종료");

// 색상 변환 함수
function hexToRGB(hex) {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return { r, g, b };
}

function updateCanvas(data) {
  const imageData = new ImageData(new Uint8ClampedArray(data), 100, 100);
  ctx.putImageData(imageData, 0, 0);
}
