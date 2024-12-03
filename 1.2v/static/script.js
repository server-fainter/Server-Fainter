const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const clientCnt = document.getElementById("client-count");
const colorPalette = document.getElementById('color-palette');

// 캔버스를 흰색으로 초기화
ctx.fillStyle = '#FFFFFF';
ctx.fillRect(0, 0, canvas.width, canvas.height);

// 색상 목록
const colorsHex = [
    "#ff0000", "#ffa600", "#ffe600", "#8aff00", "#24ff00",
    "#00ff00", "#00ff83", "#00ffe6", "#00e7ff", "#008fff",
    "#0024ff", "#2400ff", "#8a00ff", "#ff00ff", "#ff0083",
    "#ff0024", "#d70000", "#8c4000", "#e67c00", "#b1d700",
    "#4cc100", "#00c16a", "#00c1c1", "#0055c1", "#002fc1",
    "#7200c1", "#c10072", "#8c2600", "#000000", "#ffffff",
];

// 선택된 색상 변수
let selectedColorIndex = 29; // 기본값: 흰색

// 팔레트 생성
colorsHex.forEach((color, index) => {
    const colorBox = document.createElement("div");
    colorBox.className = "color-box";
    colorBox.style.backgroundColor = color;

    // 클릭 이벤트
    colorBox.addEventListener("click", () => {
        selectedColorIndex = index; // 선택된 색상의 인덱스 업데이트
        document.querySelectorAll(".color-box").forEach(box => {
            box.classList.remove("active");
        });
        colorBox.classList.add("active");
        console.log(`선택된 색상 인덱스: ${selectedColorIndex}`);
    });

    colorPalette.appendChild(colorBox);
});

// WebSocket 서버에 연결
const ws = new WebSocket('ws://localhost:8080');

ws.onopen = function() {
    console.log('WebSocket 연결이 열렸습니다.');
};

ws.onmessage = function(event) {
    console.log('recv:', event.data);
    try {
        const data = JSON.parse(event.data);

        if (data.updated_pixel && Array.isArray(data.updated_pixel)) {
            data.updated_pixel.forEach(pixel => {
                const x = pixel.x;
                const y = pixel.y;
                const colorIndex = pixel.color;

                // 캔버스의 해당 픽셀 업데이트
                ctx.fillStyle = colorsHex[colorIndex];
                ctx.fillRect(x, y, 1, 1);
            });
        }

        // 클라이언트 수 업데이트
        if (data.client_count !== undefined) {
            clientCnt.textContent = data.client_count;
        }

    } catch (e) {
        console.error('JSON 파싱 오류:', e);
    }
};

ws.onclose = function() {
    console.log('WebSocket 연결이 종료되었습니다.');
};

canvas.addEventListener('click', function(event) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = Math.floor((event.clientX - rect.left) * scaleX);
    const y = Math.floor((event.clientY - rect.top) * scaleY);

    // 캔버스의 해당 픽셀 업데이트
    ctx.fillStyle = colorsHex[selectedColorIndex];
    ctx.fillRect(x, y, 1, 1);

    // 색상 인덱스를 포함한 메시지 생성
    const message = JSON.stringify({ pixel: { x: x, y: y, color: selectedColorIndex } });
    console.log('send:', message);

    if (ws.readyState === WebSocket.OPEN) {
        ws.send(message);
    } else {
        console.warn('WebSocket이 열려 있지 않습니다. 메시지를 보낼 수 없습니다.');
    }
});
