# SERVER-FAINTER
![image](https://github.com/user-attachments/assets/0de63c88-c24e-4fb6-b716-0d6ee45d7a2f)

- **프로젝트명 :** FAINTER
    - 거대한 캔버스를 모든 사용자가 함께 채워나가는 웹 서비스,
    그리고 이를 받치는 안정적인 서버 제작을 목표로 합니다.
- **개발기간 :** 2024.11~2024.12.04
- **개발언어:** js,css,html, **C**

---

### 주요구현기술

- **웹 소켓 구현**
    - libwebsocket.h 으로 사용할 수 있는 웹소켓 기능을 라이브러리 없이 직접 구현
- **멀티쓰레딩**
    - 멀티쓰레딩을 통한 업무 분할로 클라이언트 요청 및 브로드캐스팅 빠른 처리
- **HTTP 요청처리**
    - 웹 클라이언트와의 핸드쉐이킹을 위한 HTTP 프레임 파싱 및 처리
- **non blocking IO 구현**
    - 소켓 통신 동안의 병목현상 해결
- 외부네트워크 접근
    - AWS EC2를 통해 외부 네트워크서 접근가능

### 최종 버전

![image](https://github.com/user-attachments/assets/1c7037f0-4096-44b3-a91a-268f9ef18970)


**- 1.2 v**

- **개선점**
    - TCP Socket 서버에서 Websocket 기능을 추가로 구현
    - 서버 안정화 - 예외 처리, 메모리 누수
- **의의**
    - 웹 브라우저에서 직접 TCP Socket 서버(1.0v, 1.1v)에 접속이 불가능하였지만, Websocket을 구현함으로써 접속가능해짐
