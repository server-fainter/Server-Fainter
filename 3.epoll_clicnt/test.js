import ws from 'k6/ws';
import { check, sleep } from 'k6';

export let options = {
  stages: [
    { duration: '1m', target: 1000 }, // 1분 동안 VU를 100명까지 늘립니다.
    { duration: '1m', target: 1000 }, // 5분 동안 VU 100명을 유지합니다.
    { duration: '1m', target: 0 },   // 1분 동안 VU를 0명으로 줄입니다.
  ],
};

export default function () {
  const url = 'ws://localhost:8080';
  const res = ws.connect(url, null, function (socket) {
    socket.on('open', function () {
      console.log('연결 성공');

      // 초기 픽셀 업데이트 전송
      socket.sendBinary(generatePixelUpdate());

      // 10초마다 픽셀 업데이트를 전송합니다.
      let sendInterval = setInterval(function () {
        socket.sendBinary(generatePixelUpdate());
      }, 1000000);

      // 60초 후에 연결을 종료합니다.
      socket.setTimeout(function () {
        console.log('소켓을 닫습니다');
        clearInterval(sendInterval);
        socket.close();
      }, 6000000);
    });

    socket.on('binaryMessage', function (data) {
      // 필요한 경우 수신된 바이너리 메시지를 처리합니다.
      //console.log(`수신된 바이너리 메시지 길이: ${data.byteLength}`);
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
  // 랜덤한 x, y 좌표와 색상 인덱스를 생성합니다.
  const x = Math.floor(Math.random() * 500);
  const y = Math.floor(Math.random() * 500);
  const color = Math.floor(Math.random() * 30); // 색상 인덱스는 0부터 29까지

  const buffer = new ArrayBuffer(5);
  const view = new DataView(buffer);

  view.setUint16(0, x, true); // 리틀 엔디언
  view.setUint16(2, y, true);
  view.setUint8(4, color);

  return buffer;
}
