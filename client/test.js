// test_script.js

import ws from 'k6/ws';
import { check, sleep } from 'k6';
import { Trend } from 'k6/metrics';

// 응답 시간 측정을 위한 트렌드 메트릭
let responseTimeTrend = new Trend('websocket_response_time');

// 테스트 옵션 설정
export let options = {
    stages: [
        { duration: '15s', target: 2000 }, // Ramp-up: 100 VUs로 10초 동안 증가
        { duration: '45s', target: 2000 }, // 유지: 100 VUs로 60초 동안 유지
        { duration: '15s', target: 0 },   // Ramp-down: 10초 동안 0으로 감소
    ],
};

export default function () {
    const url = 'ws://localhost:8081'; // WebSocket 서버 URL
    const vu = __VU; // 현재 가상 사용자 번호

    ws.connect(url, {}, function (socket) {
        let sentPixels = [];
        const testDuration = 45; // 테스트 지속 시간 (초)
        const interval = 0.5; // 메시지 전송 간격 (초)
        const endTime = Date.now() + testDuration * 1000;

        socket.on('open', function () {
            //console.log(`VU ${vu}: WebSocket 연결 열림`);

            // 메시지 전송 및 대기 루프
            while (Date.now() < endTime) {
                // 클라이언트에서 보낼 픽셀 JSON 생성
                const x = Math.floor(Math.random() * 100); // 0 ~ 99 랜덤
                const y = Math.floor(Math.random() * 100); // 0 ~ 99 랜덤
                const color = `#${Math.floor(Math.random() * 16777215).toString(16).padStart(6, '0')}`; // 랜덤 HEX 색상

                const pixelData = { pixel: { x: x, y: y, color: color } };
                const message = JSON.stringify(pixelData); // 메시지를 JSON 문자열로 변환

                const sendTime = Date.now(); // 메시지 전송 시점 기록

                socket.send(message); // 메시지 전송

                //console.log(`VU ${vu}: send : ${message}`);

                // 보낸 픽셀 정보를 저장
                sentPixels.push({ x: x, y: y, color: color, sendTime: sendTime });

                sleep(interval); // 다음 메시지 전송까지 대기
            }

            socket.close(); // 테스트 종료 후 연결 닫기
        });

        // 응답 메시지 처리
        socket.on('message', function (response) {
            console.log(`VU ${vu}: 응답 메시지: ${response}`);

            try {
                const data = JSON.parse(response);
                if (data.updated_pixel && Array.isArray(data.updated_pixel)) {
                    data.updated_pixel.forEach((pixel) => {
                        // 수신한 픽셀이 보낸 픽셀 중에 있는지 확인
                        const matchedIndex = sentPixels.findIndex((p) => {
                            return p.x === pixel.x && p.y === pixel.y && p.color.toLowerCase() === pixel.color.toLowerCase();
                        });

                        if (matchedIndex !== -1) {
                            const sendTime = sentPixels[matchedIndex].sendTime;
                            const receiveTime = Date.now(); // 응답 수신 시점 기록
                            const responseTime = receiveTime - sendTime; // 응답 시간 계산
                            responseTimeTrend.add(responseTime); // 응답 시간 저장

                            // 매칭된 경우 체크 통과
                            check(pixel, {
                                '서버 응답에 보낸 픽셀이 포함됨': (p) => true,
                            });

                            // 이미 매칭된 픽셀은 리스트에서 제거하여 중복 체크 방지
                            sentPixels.splice(matchedIndex, 1);
                        }
                    });
                }
            } catch (e) {
                console.error(`VU ${vu}: 응답 처리 중 오류 발생: ${e}`);
            }
        });

        socket.on('error', function (e) {
            console.error(`VU ${vu}: WebSocket 에러 발생: ${e.error()}`);
        });

        socket.on('close', function () {
            console.log(`VU ${vu}: WebSocket 연결 닫힘`);
        });
    });

    // VU가 테스트 기간 동안 활성 상태를 유지하도록 대기
    sleep(80); // 테스트 지속 시간(60s) + Ramp-up/Ramp-down 시간 여유
}
