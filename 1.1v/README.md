1. nodejs 설치가 필요함(중계서버용)
sudo apt-get install -y nodejs 

2. make 

3. ./server 실행

4. test_src 안의 proxyserver.js를 실행 (중계서버)
4-1. 만약 실행 안된다면 npm install ws 를 통해 웹소켓 모듈 설치가 필요함 

5. test_src 안의 index.html 를 웹브라우저에서 실행시키면 테스트 가능 (요청 및 브로드캐스팅 테스트 가능)
