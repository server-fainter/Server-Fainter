import ws from 'k6/ws';
import { check, sleep } from 'k6';

export let options = {
  stages: [
    { duration: '20s', target: 1000 },
    { duration: '1m', target: 1000 },
    { duration: '1m', target: 0 },  
  ],
};

export default function () {
  const url = 'ws://localhost:8080';
  const res = ws.connect(url, null, function (socket) {
    socket.on('open', function () {
      console.log('연결 성공');

      // 초기 픽셀 업데이트 전송
      socket.sendBinary(generatePixelUpdate());

      let sendInterval = setInterval(function () {
        socket.sendBinary(generatePixelUpdate());
      }, 100);

      socket.setTimeout(function () {
        console.log('소켓을 닫습니다');
        clearInterval(sendInterval);
        socket.close();
      }, 600000);
    });

    socket.on('binaryMessage', function (data) {
    });

    socket.on('close', function () {
      console.log('연결 종료');
    });

    socket.on('error', function (e) {
      console.error('WebSocket 오류:', e);
    });
  });

  check(res, { 'status is 101': (r) => r && r.status === 101 });
}

function generatePixelUpdate() {
  // 랜덤한 x, y 좌표와 색상 인덱스를 생성
  const x = Math.floor(Math.random() * 500);
  const y = Math.floor(Math.random() * 500);
  const color = Math.floor(Math.random() * 30); // 색상 인덱스는 0부터 29까지

  const buffer = new ArrayBuffer(5);
  const view = new DataView(buffer);

  view.setUint16(0, x, true); 
  view.setUint16(2, y, true);
  view.setUint8(4, color);

  return buffer;
}
