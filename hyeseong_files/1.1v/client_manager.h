#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <sys/epoll.h>
#include <task_queue.h>
#include <pthread.h>

// 클라이언트 정보를 담는 구조체
typedef struct Client {
    int socket_fd;              // 클라이언트 소켓 파일 디스크립터
    struct Client* next;        // 다음 클라이언트를 가리키는 포인터
} Client;

// 클라이언트 매니저 구조체
typedef struct {
    Client* head;                        // 연결 리스트의 시작점
    int server_socket;                   // 서버 소켓 파일 디스크립터
    int port_number;                     // 서버 포트 번호
    int epoll_fd;                        // epoll 인스턴스 파일 디스크립터
    struct epoll_event ev;               // epoll에 등록된 이벤트
    struct epoll_event *events;          // epoll에서 감지된 이벤트 리스트
    pthread_t tid;                       // 클라이언트 매니저 스레드
    TaskQueue *queue;                     // Task Queue
} ClientManager;

int set_nonblocking(const int fd);

// 클라이언트 매니저 초기화
int initClientManager(ClientManager* manager, const int port, const int events_size, const int queue_size);

// 클라이언트 추가
void addClient(ClientManager* manager);

// 클라이언트 제거
int removeClient(ClientManager* manager,const int client_fd);

// 모든 클라이언트에게 메시지 보내기
void broadcastClients(ClientManager* manager, char* message);

// 클라이언트 매니저 정리 (모든 클라이언트 제거 및 메모리 해제)
void destroyClientManger(ClientManager* manager);

#endif // CLIENT_MANAGER_H
