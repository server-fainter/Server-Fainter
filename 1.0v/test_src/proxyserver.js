const net = require('net');
const WebSocket = require('ws');

const tcpServerHost = 'localhost'; // TCP 소켓 서버의 주소
const tcpServerPort = 8080;       // TCP 소켓 서버의 포트

const wssPort = 8081; // WebSocket 서버의 포트

// WebSocket 서버 생성
const wss = new WebSocket.Server({ port: wssPort }, () => {
    console.log(`WebSocket 서버가 포트 ${wssPort}에서 실행 중입니다.`);
});

// TCP 소켓 서버에 연결
const tcpClient = new net.Socket();

tcpClient.connect(tcpServerPort, tcpServerHost, () => {
    console.log('TCP 소켓 서버에 연결되었습니다.');
});

let tcpBuffer = '';

// tcpClient.on('data', (data) => {
//     tcpBuffer += data.toString();

//     // console.log('recv : ', data.toString());

//     // 메시지를 개행 문자로 분리 (\n)
//     let messages = tcpBuffer.split('\n');
//     tcpBuffer = messages.pop(); // 마지막 부분적인 메시지는 버퍼에 유지

//     messages.forEach((message) => {
//         if (message.trim() !== '') {
//             console.log('TCP 서버로부터 수신:', message);

//             // 연결된 모든 WebSocket 클라이언트에게 메시지 전달
//             wss.clients.forEach(function each(client) {
//                 if (client.readyState === WebSocket.OPEN) {
//                     client.send(message);
//                 }
//             });
//         }
//     });
// });

tcpClient.on('close', () => {
    console.log('TCP 서버와의 연결이 종료되었습니다.');
});

wss.on('connection', (ws) => {
    console.log('클라이언트가 연결되었습니다.');

    ws.on('message', (message) => {
        console.log('클라이언트로부터 수신:', message);

        // 메시지를 TCP 소켓 서버로 전달 (개행 문자 추가)
        tcpClient.write(message + '\n');
    });

    ws.on('close', () => {
        console.log('클라이언트 연결이 종료되었습니다.');
    });
});
