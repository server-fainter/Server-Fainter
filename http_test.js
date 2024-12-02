import http from 'k6/http';
import { check } from 'k6';

export let options = {
  stages: [
    { duration: '1s', target: 500 },  // 1초 동안 500명의 가상 사용자로 요청 보냄
    { duration: '29s', target: 500 }, // 29초 동안 500명으로 지속적으로 테스트
  ],
};

export default function () {
  let requests = [];

  // 10개의 요청을 한번에 보내는 배치 처리
  for (let i = 0; i < 10; i++) {
    let x = Math.floor(Math.random() * 100);  // 0부터 99까지 랜덤 x 좌표
    let y = Math.floor(Math.random() * 100);  // 0부터 99까지 랜덤 y 좌표

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

    requests.push(http.post('http://localhost:8080/update-pixel', payload, params));
  }

  // 배치 요청 보내기
  let responses = http.batch(requests);

  // 각 응답에 대해 체크
  responses.forEach(response => {
    check(response, {
      'is status 200': (r) => r.status === 200,
    });
  });

  // 요청 간 간격을 0초로 설정
  sleep(0); 
}
