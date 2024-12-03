#ifndef SERVER_H
#define SERVER_H

#include <sys/epoll.h>

#define PORT 8080           // 서버 포트
#define MAX_EVENTS 2048     // epoll 최대 이벤트 수
#define BUFFER_SIZE 1024    // 클라이언트와의 메시지 버퍼 크기

// 서버 매니저 구조체 정의
typedef struct {
    int server_socket;                   // 서버 소켓 파일 디스크립터
    int epoll_fd;                        // epoll 인스턴스 파일 디스크립터
    struct epoll_event ev;               // epoll에 등록된 이벤트
    struct epoll_event events[MAX_EVENTS]; // epoll에서 감지된 이벤트 리스트
} ServerManager;

// 파일 디스크립터를 non-blocking 모드로 설정하는 함수
int set_nonblocking(int fd);

// 서버 초기화 함수
void init_server(ServerManager *sm);

// 클라이언트 연결 수락 함수
int accept_new_client(ServerManager *sm);

// 서버 종료 함수
void close_server(ServerManager *sm);

#endif // SERVER_H
