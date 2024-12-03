import { check, sleep } from "k6";
import ws from "k6/ws";
import { Trend, Counter } from "k6/metrics";

// 사용자 정의 메트릭
let latency = new Trend("ws_latency", true); // 응답 시간 측정
let throughput = new Counter("ws_throughput"); // 처리량 측정
let errors = new Counter("ws_errors"); // 오류 카운트

export let options = {
  stages: [
    { duration: "10s", target: 50 }, // 30초 동안 VU를 50명까지 증가
    { duration: "5s", target: 0 },  // 10초 동안 VU를 0으로 감소
  ],
};

export default function () {
  const url = "ws://localhost:8080"; // 테스트할 웹소켓 서버 URL

  const res = ws.connect(url, {}, function (socket) {
    let pendingPixels = []; // 전송된 픽셀과 전송 시간을 저장할 배열

    // 무작위로 x, y, color 값을 생성하는 함수
    function generateRandomPixel() {
      return {
        x: Math.floor(Math.random() * 1000), // 0에서 999 사이의 정수
        y: Math.floor(Math.random() * 1000),
        color: Math.floor(Math.random() * 30), // 0에서 29 사이의 정수
      };
    }

    socket.on("open", function () {
      console.log("웹소켓 연결이 열렸습니다.");

      // 주기적으로 메시지를 보내는 함수
      function sendMessage() {
        // 전송할 픽셀 데이터 생성
        const pixel = generateRandomPixel();
        const pixelData = { pixel: pixel };
        const pixelDataString = JSON.stringify(pixelData);

        // 메시지 전송 시간 기록
        const sendTime = new Date().getTime();
        pendingPixels.push({ pixel: pixel, sendTime: sendTime });

        // 메시지 전송
        socket.send(pixelDataString);

        // 다음 메시지 전송까지의 간격 설정 (예: 500ms)
        socket.setTimeout(sendMessage, 500); // 500ms 후에 다시 호출
      }

      // 처음 메시지 전송 시작
      sendMessage();
    });

    socket.on("message", function (message) {
      //console.log("수신 데이터:", message); // 수신한 데이터 로그 출력

      try {
        const json = JSON.parse(message);
        // JSON 데이터 처리
        //console.log("파싱된 JSON:", json);

        if (json.updated_pixel && Array.isArray(json.updated_pixel)) {
          const currentTime = new Date().getTime();
          // 전송된 픽셀 중 서버 응답에 포함된 픽셀을 확인
          for (let i = pendingPixels.length - 1; i >= 0; i--) {
            const sentPixel = pendingPixels[i];
            // 서버의 updated_pixel 배열에 전송한 픽셀이 있는지 확인
            const isPixelAcknowledged = json.updated_pixel.some(
              (updatedPixel) =>
                updatedPixel.x === sentPixel.pixel.x &&
                updatedPixel.y === sentPixel.pixel.y &&
                updatedPixel.color === sentPixel.pixel.color
            );

            if (isPixelAcknowledged) {
              // 응답 시간 계산
              const latencyValue = currentTime - sentPixel.sendTime;
              latency.add(latencyValue); // 응답 시간 메트릭에 추가
              throughput.add(1); // 처리량 증가
              // 확인된 픽셀을 pendingPixels 배열에서 제거
              pendingPixels.splice(i, 1);
            }
          }
        }
      } catch (e) {
        console.error("JSON 파싱 오류:", e.message);
        errors.add(1); // JSON 파싱 오류 메트릭 증가
      }
    });

    socket.on("close", function () {
      console.log("웹소켓 연결이 종료되었습니다.");
    });

    socket.on("error", function (e) {
      console.error(`웹소켓 오류: ${e.error()}`);
      errors.add(1); // 웹소켓 오류 메트릭 증가
    });

    // 일정 시간 후 연결 종료 (예: 30초)
    socket.setTimeout(function () {
      console.log("30초 후에 소켓을 닫습니다.");
      socket.close();
    }, 30000); // 30초 후 연결 종료
  });

  // 연결 성공 여부 체크
  check(res, { "웹소켓 연결 성공": (r) => r && r.status === 101 });

  // 각 VU가 사이클마다 잠시 대기 (예: 1초)
  sleep(1);
}
