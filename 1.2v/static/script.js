// JavaScript 코드

const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const clientCnt = document.getElementById("client-count");

// 캔버스를 흰색으로 초기화
ctx.fillStyle = '#FFFFFF';
ctx.fillRect(0, 0, canvas.width, canvas.height);

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

canvas.addEventListener('click', function(event) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const x = Math.floor((event.clientX - rect.left) * scaleX);
    const y = Math.floor((event.clientY - rect.top) * scaleY);

    const color = `#${Math.floor(Math.random() * 16777215).toString(16).padStart(6, '0')}`; // 랜덤 색상 사용

    // 캔버스의 해당 픽셀 업데이트
    ctx.fillStyle = color;
    ctx.fillRect(x, y, 1, 1);

    const message = JSON.stringify({ pixel: { x: x, y: y, color: color } });
    console.log('send:', message);

    ws.send(message);
});
