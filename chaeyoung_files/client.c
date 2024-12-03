#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define SER_PORT 8080
#define SER_IP "127.0.0.1"

int main() {
    // 소켓 생성
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return -1;
    }

    // 서버 주소 설정
    struct sockaddr_in ser_addr;
    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(SER_PORT);
    ser_addr.sin_addr.s_addr = inet_addr(SER_IP);

    // 서버에 연결 시도
    if (connect(sock, (struct sockaddr*)&ser_addr, sizeof(ser_addr)) == -1) {
        perror("Connection to server failed");
        close(sock);
        return -1;
    }

    printf("Connected to server at %s:%d\n", SER_IP, SER_PORT);

    // 서버에서 `cli.html`을 브라우저로 열기 위한 명령 실행
    printf("Opening browser...\n");

    // WSL 환경에서 브라우저 열기 (Windows 환경)
    char exec[512];
    snprintf(exec, sizeof(exec), "cmd.exe /C start http://%s:%d", SER_IP, SER_PORT);

    // 명령 실행
    if (system(exec) == -1) {
        perror("Failed to open browser");
    }

    // 소켓 닫기
    close(sock);
    return 0;
}