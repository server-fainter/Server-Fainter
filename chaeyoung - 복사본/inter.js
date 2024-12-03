// // 클라이언트에서 1초마다 서버에 GET 요청 보내기
// setInterval(function() {
//     fetch('/get-existing-pixels') // 서버에서 기존 픽셀 데이터 요청
//         .then(response => response.json()) // JSON 형태로 응답 받기
//         .then(data => {
//             console.log(data); // 데이터가 올바른지 확인 (디버깅용)
//             // 데이터 받아서 캔버스에 반영하기
//             updateCanvas(data);
//         })
//         .catch(error => {
//             console.error('Error:', error);
//         });
// }, 1000);

// // 캔버스를 갱신하는 함수 (데이터 기반)
// function updateCanvas(data) {
//     const canvas = document.getElementById('canvas');
//     const ctx = canvas.getContext('2d');
    
//     // pixels 배열에 있는 각 픽셀을 캔버스에 반영
//     data.pixels.forEach(pixel => {
//         ctx.fillStyle = pixel.color;  // 픽셀 색상
//         ctx.fillRect(pixel.x, pixel.y, 1, 1);  // 픽셀 위치와 크기 (1x1)
//     });
// }
