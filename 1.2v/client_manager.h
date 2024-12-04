#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <sys/epoll.h>
#include <task_queue.h>
#include <pthread.h>
#include <stdbool.h>

#define REQUEST_BUFFER_SIZE 1024 * 4 // 4KB
#define STATIC_FILES_DIR "./static"


typedef enum {
    CONNECTION_HANDSHAKE,  // 초기 핸드셰이크 단계
    CONNECTION_OPEN,       // WebSocket 연결 완료 상태
    CONNECTION_CLOSING,    // 연결 종료 중
    CONNECTION_CLOSED      // 연결 종료 상태
} ConnectionState;

// 클라이언트 정보를 담는 구조체
typedef struct Client {
    int socket_fd;              // 클라이언트 소켓 파일 디스크립터
    struct Client* next;        // 다음 클라이언트를 가리키는 포인터

    // websocket을 위해 추가한 것
    ConnectionState state;                      // 연결 상태
    char recv_buffer[REQUEST_BUFFER_SIZE];      // 수신 버퍼
    size_t recv_buffer_len;                     // 수신 버퍼에 저장된 데이터 길이
    bool incomplete_http;                       // http 요청 조각 상태
    bool incomplete_frame;                      // frame 요청 조각 상태
    
} Client;

// 클라이언트 매니저 구조체
typedef struct {
    Client* head;                        // 연결 리스트의 시작점
    TaskQueue* canvas_queue;             // 캔버스 Task Queue
    int server_socket;                   // 서버 소켓 파일 디스크립터
    int port_number;                     // 서버 포트 번호
    int epoll_fd;                        // epoll 인스턴스 파일 디스크립터
    struct epoll_event ev;               // epoll에 등록된 이벤트
    struct epoll_event *events;          // epoll에서 감지된 이벤트 리스트
    pthread_t tid;                       // 클라이언트 매니저 스레드
    TaskQueue *queue;                    // Task Queue
    int client_count;                    // 접속한 클라이언트 수
    pthread_spinlock_t lock;
} ClientManager;

int process_buffer(ClientManager *manager, Client *client, char *buffer, size_t len);

int set_nonblocking(const int fd);

// 클라이언트 매니저 초기화
int initClientManager(ClientManager* manager, TaskQueue *canvas_queue, const int port, const int events_size, const int queue_size);

// 클라이언트 추가
void addClient(ClientManager* manager);

// 클라이언트 제거
int removeClient(ClientManager* manager, const int client_fd);

// 모든 클라이언트에게 메시지 보내기
void broadcastClients(ClientManager* manager, char* message, size_t message_len);

// 클라이언트 매니저 정리 (모든 클라이언트 제거 및 메모리 해제)
void destroyClientManger(ClientManager* manager);

// client_socket(파일 디스크립터)를 통해 client를 찾는 함수 
Client* find_client(ClientManager* manager, int fd);
int get_client_count(ClientManager* manager);
#endif // CLIENT_MANAGER_H
