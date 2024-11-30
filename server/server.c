#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/epoll.h>


// 파일 디스크립터를 논블로킹 모드로 설정
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

// 서버 초기화
void init_server(ServerManager *sm){

    // 서버 소켓 생성
    sm->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (sm->server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 서버 소켓을 논블로킹 모드로 설정
    if (set_nonblocking(sm->server_socket) == -1) {
        close(sm->server_socket);
        exit(EXIT_FAILURE);
    }

    // 서버 주소 구조체 초기화
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 소켓 바인딩
    if (bind(sm->server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(sm->server_socket);
        exit(EXIT_FAILURE);
    }

    // 소켓 리슨
    if (listen(sm->server_socket, SOMAXCONN) == -1) {
        perror("listen failed");
        close(sm->server_socket);
        exit(EXIT_FAILURE);
    }

    // epoll 파일 디스크립터 생성
    sm->epoll_fd = epoll_create1(0);
    if (sm->epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(sm->server_socket);
        exit(EXIT_FAILURE);
    }

    // 서버 소켓을 epoll에 등록
    sm->ev.events = EPOLLIN | EPOLLET; // 읽기 이벤트 + Edge Triggered
    sm->ev.data.fd = sm->server_socket;
    if (epoll_ctl(sm->epoll_fd, EPOLL_CTL_ADD, sm->server_socket, &sm->ev) == -1) {
        perror("epoll_ctl failed");
        close(sm->server_socket);
        close(sm->epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server initialized. Listening on port %d\n", PORT);
}

// 새로운 클라이언트 연결 수락
int accept_new_client(ServerManager *sm) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(sm->server_socket, (struct sockaddr *)&client_addr, &client_len);

    if (client_socket == -1) {
        perror("accept failed");
        return -1;
    }

    // 클라이언트 소켓을 논블로킹 모드로 설정
    if (set_nonblocking(client_socket) == -1) {
        close(client_socket);
        return -1;
    }

    // 클라이언트 소켓을 epoll에 등록
    sm->ev.events = EPOLLIN | EPOLLET; // 읽기 이벤트 + Edge Triggered
    sm->ev.data.fd = client_socket;
    if (epoll_ctl(sm->epoll_fd, EPOLL_CTL_ADD, client_socket, &sm->ev) == -1) {
        perror("epoll_ctl add client failed");
        close(client_socket);
        return -1;
    }

    printf("New Client Conneted: IP = %s, Port = %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return client_socket;
}

// 서버 종료
void close_server(ServerManager *sm) {
    close(sm->server_socket);
    close(sm->epoll_fd);
    printf("Server resources released.\n");
}
