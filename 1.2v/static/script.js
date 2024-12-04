const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const clientCnt = document.getElementById("client-count");
const colorPicker = document.getElementById('color-picker');
const recentColorsPalette = document.getElementById('recent-colors');

// 선택된 색상 변수
let selectedColor = '#ffffff'; // 기본값: 흰색

// 최근 사용한 색상을 저장할 배열
let recentColors = [
     "#ff0000",
          "#ffa600",
          "#ffe600",
          "#8aff00",
          "#24ff00",
          "#00ff00",
          "#00ff83",
          "#00ffe6",
          "#00e7ff",
          "#000000"
];
const maxRecentColors = 10; // 최대 저장 개수

// 색상 선택기 이벤트 리스너
colorPicker.addEventListener('input', function(event) {
    selectedColor = event.target.value;
    addRecentColor(selectedColor);
});

// 최근 사용한 색상을 팔레트에 추가하는 함수
function addRecentColor(color) {
    // 이미 존재하는 색상이라면 제거
    const index = recentColors.indexOf(color);
    if (index !== -1) {
        recentColors.splice(index, 1);
    }
    recentColors.unshift(color); // 배열의 앞에 추가
    // 최대 개수 초과 시 마지막 색상 제거
    if (recentColors.length > maxRecentColors) {
        recentColors.pop();
    }
    updateRecentColorsPalette();
}

// 최근 사용한 색상 팔레트를 업데이트하는 함수
function updateRecentColorsPalette() {
    recentColorsPalette.innerHTML = '';
    recentColors.forEach(color => {
        const colorBox = document.createElement("div");
        colorBox.className = "color-box";
        colorBox.style.backgroundColor = color;

        // 클릭 이벤트로 색상 선택
        colorBox.addEventListener("click", () => {
            selectedColor = color;
            colorPicker.value = color; // 색상 선택기의 값도 업데이트
            console.log(`선택된 색상: ${selectedColor}`);
        });

        recentColorsPalette.appendChild(colorBox);
    });
}
updateRecentColorsPalette();
// WebSocket 서버에 연결
const ws = new WebSocket('ws://localhost:8080');

ws.onopen = function() {
    console.log('WebSocket 연결이 열렸습니다.');

};

ws.onmessage = function(event) {
    console.log('recv:', event.data);
    try {
        const data = JSON.parse(event.data);

        // 서버로부터 초기화 메시지를 받은 경우
        if (data.init) {
            initializeCanvas(data.init);
        }

        if (data.updated_pixel && Array.isArray(data.updated_pixel)) {
            data.updated_pixel.forEach(pixel => {
                const x = pixel.x;
                const y = pixel.y;
                const color = pixel.color;

                // 캔버스의 해당 픽셀 업데이트
                ctx.fillStyle = color;
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

// 캔버스 초기화 함수
function initializeCanvas(initData) {
    const width = initData.width;
    const height = initData.height;
    const pixels = initData.pixels; // 색상 문자열의 배열

    // 캔버스 크기 설정
    canvas.width = width;
    canvas.height = height;

    // 화면에 보여질 캔버스 크기는 CSS로 조정하므로 여기서는 설정하지 않음
    // canvas.style.width = width + 'px';
    // canvas.style.height = height + 'px';

    // 픽셀 데이터를 사용하여 캔버스 그리기
    // pixels 배열은 일차원 배열이며, 각 요소는 "#RRGGBB" 형식의 색상 문자열입니다.
    // 일차원 배열을 이차원 픽셀 배열로 매핑하여 그립니다.
    const imageData = ctx.createImageData(width, height);
    for (let i = 0; i < pixels.length; i++) {
        const colorStr = pixels[i];
        const x = i % width;
        const y = Math.floor(i / width);

        // 색상 문자열을 RGB 값으로 변환
        const rgb = hexToRgb(colorStr);

        // 이미지 데이터에 픽셀 설정
        const index = (y * width + x) * 4;
        imageData.data[index] = rgb.r;
        imageData.data[index + 1] = rgb.g;
        imageData.data[index + 2] = rgb.b;
        imageData.data[index + 3] = 255; // 알파 값 (불투명)
    }
    // 캔버스에 이미지 데이터 그리기
    ctx.putImageData(imageData, 0, 0);
}

// HEX 색상 문자열을 RGB 객체로 변환하는 함수
function hexToRgb(hex) {
    // '#RRGGBB' 또는 'RRGGBB' 형식의 문자열을 처리
    let cleanHex = hex.replace('#', '');
    if (cleanHex.length === 3) {
        // '#RGB' 형식의 문자열 처리
        cleanHex = cleanHex.split('').map(char => char + char).join('');
    }
    const bigint = parseInt(cleanHex, 16);
    const r = (bigint >> 16) & 255;
    const g = (bigint >> 8) & 255;
    const b = bigint & 255;
    return { r, g, b };
}

canvas.addEventListener('click', function(event) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = Math.floor((event.clientX - rect.left) * scaleX);
    const y = Math.floor((event.clientY - rect.top) * scaleY);

    // 캔버스의 해당 픽셀 업데이트
    ctx.fillStyle = selectedColor;
    ctx.fillRect(x, y, 1, 1);

    // 색상 값을 포함한 메시지 생성
    const message = JSON.stringify({ pixel: { x: x, y: y, color: selectedColor } });
    console.log('send:', message);

    if (ws.readyState === WebSocket.OPEN) {
        ws.send(message);
    } else {
        console.warn('WebSocket이 열려 있지 않습니다. 메시지를 보낼 수 없습니다.');
    }
});
