import http from 'k6/http';
import { check, sleep } from 'k6';
import { Trend, Counter, Rate } from 'k6/metrics';

// 테스트 옵션 설정
export let options = {
  stages: [
    { duration: '10s', target: 100 },    // 10초 동안 100명으로 증가
    { duration: '50s', target: 500 },    // 다음 50초 동안 500명으로 증가
    { duration: '50s', target: 1000 },   // 다음 50초 동안 1000명으로 증가
    { duration: '10s', target: 1000 },   // 10초 동안 1000명 유지
    { duration: '1m', target: 500 },     // 1분 동안 500명으로 감소
    { duration: '1m', target: 100 },     // 다음 1분 동안 100명으로 감소
    { duration: '20s', target: 0 },      // 마지막 20초 동안 0명으로 감소
  ],
  thresholds: {
    'http_request_duration': ['p(95)<500'],      // 상위 95%의 요청 시간이 500ms 미만
    'http_success_rate': ['rate>0.95'],          // 95% 이상의 요청이 성공
    'http_requests_total': ['count>1000'],       // 총 1000개 이상의 요청
    'http_failures': ['count<50'],               // 실패 요청이 50개 미만
  },
};

// 메트릭 정의
const httpRequestDuration = new Trend('http_request_duration'); // 요청 소요 시간
const httpSuccessRate = new Rate('http_success_rate');         // 성공 비율
const httpRequestsTotal = new Counter('http_requests_total');   // 총 요청 수
const httpFailures = new Counter('http_failures');             // 실패 요청 수

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
      // 필요한 경우 추가 헤더
    },
  };

  try {
    // POST 요청: 랜덤 픽셀 업데이트
    let response = http.post('http://localhost:8080/update-pixel', payload, params);
    
    // 응답 시간 기록
    httpRequestDuration.add(response.timings.duration);
    
    // 요청 수 증가
    httpRequestsTotal.add(1);
    
    // 성공 비율 계산
    if (response.status === 200) {
      httpSuccessRate.add(true);
    } else {
      httpSuccessRate.add(false);
      httpFailures.add(1);
    }

    // 응답 상태 코드가 200인지 체크
    let result = check(response, {
      'is status 200': (r) => r.status === 200,
    });

    if (result) {
      console.log(`✅ 성공: 상태 ${response.status} - ${new Date().toISOString()}`);
    } else {
      console.error(`❌ 실패: 상태 ${response.status} - ${new Date().toISOString()}`);
    }
  } catch (e) {
    // 네트워크 오류 등 예외 처리
    httpFailures.add(1);
    console.error(`❌ 예외 발생: ${e.message} - ${new Date().toISOString()}`);
  }

  sleep(1); // 각 요청 사이에 1초 대기
}
