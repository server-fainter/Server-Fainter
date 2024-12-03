import http from 'k6/http';
import { check, sleep } from 'k6';

export let options = {
  stages: [
    { duration: '1m30s', target: 700 },  // 1초 동안 300명의 가상 사용자로 요청 보냄
    { duration: '30s', target: 0 }, // 29초 동안 300명으로 지속적으로 테스트
  ],
};

// stages: [
//     { duration: '30s', target: 100 }, // 30초 동안 100명 도달
//     { duration: '1m', target: 1000 }, // 1분 동안 1000명 도달
//     { duration: '30s', target: 0 },   // 종료
//   ],
// };
  

// 1초에 300개의 요청이 보내지도록 설정
export default function () {
  // 동적 픽셀 데이터 생성
  let x = Math.floor(Math.random() * 100);  // 0부터 99까지 랜덤 x 좌표
  let y = Math.floor(Math.random() * 100);  // 0부터 99까지 랜덤 y 좌표

  // RGB 범위에서 랜덤 색상 생성
  let r = Math.floor(Math.random() * 256);
  let g = Math.floor(Math.random() * 256);
  let b = Math.floor(Math.random() * 256);
  let color = `rgb(${r},${g},${b})`;  // RGB 형식으로 색상 지정

  let payload = JSON.stringify({
    x: x,
    y: y,
    color: color,
  });

  let params = {
    headers: {
      'Content-Type': 'application/json',
    },
  };

  // POST 요청: 랜덤 픽셀 업데이트
  let response = http.post('http://localhost:8080/update-pixel', payload, params);

  // 응답 상태 코드가 200인지 체크
  check(response, {
    'is status 200': (r) => r.status === 200,
  });

  sleep(1); // 각 요청 사이에 1초 대기
} 