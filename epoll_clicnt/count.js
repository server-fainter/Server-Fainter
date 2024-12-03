import ws from 'k6/ws';
import { check, sleep } from 'k6';

export let options = {
  stages: [
    { duration: '1m', target: 1000 }, // 1분 동안 VU를 10,000명까지 늘립니다.
    { duration: '5m', target: 1000 }, // 5분 동안 VU 10,000명을 유지합니다.
    { duration: '1m', target: 0 },     // 1분 동안 VU를 0명으로 줄입니다.
  ],
};

export default function () {
  const url = 'ws://localhost:8080';
  const params = { /* 필요한 경우 헤더나 파라미터를 추가하세요 */ };
  
  const res = ws.connect(url, params, function (socket) {
    socket.on('open', function () {
      console.log('연결 성공');

      // 연결을 유지하고 아무 메시지도 보내지 않습니다.
      // 서버로부터 메시지를 받으면 처리할 수 있습니다.

      // 일정 시간 후에 연결을 종료합니다.
      socket.setTimeout(function () {
        console.log('소켓을 닫습니다');
        socket.close();
      }, 60000); // 60초 후에 연결 종료
    });

    socket.on('close', function () {
      console.log('연결 종료');
    });

    socket.on('error', function (e) {
      console.error('WebSocket 오류:', e);
    });
  });

  check(res, { 'WebSocket 연결 상태가 101인가': (r) => r && r.status === 101 });
}
