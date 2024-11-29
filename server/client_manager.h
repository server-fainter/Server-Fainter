#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // pthread 헤더 추가

// 클라이언트 정보를 담는 구조체
typedef struct Client {
    int socket_fd;              // 클라이언트 소켓 파일 디스크립터
    struct Client* next;        // 다음 클라이언트를 가리키는 포인터
} Client;

// 클라이언트 매니저 구조체
typedef struct {
    Client* head;           // 연결 리스트의 시작점
    pthread_mutex_t mutex;  // 뮤텍스 추가
} ClientManager;

// 클라이언트 매니저 초기화
int initClientManager(ClientManager* manager);

// 클라이언트 추가
int addClient(ClientManager* manager, int client_fd);

// 클라이언트 제거
int removeClient(ClientManager* manager, int client_fd);

// 모든 클라이언트에게 메시지 보내기
int broadcastClients(ClientManager* manager, const char* message);

// 클라이언트 매니저 정리 (모든 클라이언트 제거 및 메모리 해제)
void cleanupClientManager(ClientManager* manager);

#endif // CLIENT_MANAGER_H
