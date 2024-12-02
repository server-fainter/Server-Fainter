#include "client_manager.h"

#include "canvas.h"
#include <http_handler.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdbool.h>

static void *worker_thread(void *arg) {

    pthread_t tid = pthread_self();
    printf("ClientManager Thread : %ld\n", tid);

    ClientManager *cm = (ClientManager *)arg;
    TaskQueue *queue = cm->queue;

    while (1) {
        Task task = pop_task(queue);
        Client *client = find_client(cm, task.client);

        switch (task.type) {
            case TASK_NEW_CLIENT: {
                //printf("TASK_NEW_CLIENT\n");
                addClient(cm);
                break;
            }

            case TASK_BROADCAST: {
                broadcastClients(cm, task.data, task.data_len);
                break;
            }

            case TASK_FRAME_MESSAGE: {
                //printf("TASK_FRAME_MESSAGE\n");
                if (process_buffer(cm, client, (char *)task.data, task.data_len) == 0){

                    // 버퍼 복사해서 캔버스한테 보내줌
                    char *tmp = (char *)malloc(client->recv_buffer_len * sizeof(char));
                    memcpy(tmp, client->recv_buffer, client->recv_buffer_len);
                    Task pixel_task = {0, TASK_PIXEL_UPDATE, tmp, client->recv_buffer_len};
                    push_task(cm->canvas_queue, pixel_task);
                    client->recv_buffer_len = 0;
                    client->incomplete_frame = false;
                }
                break;
            }

            case TASK_HTTP_REQUEST: {
                //printf("TASK_HTTP_REQUEST\n");
                if (process_buffer(cm, client, (char *)task.data, task.data_len) == 0) {
                    handle_http_request(client); // HTTP 요청 처리
                    client->recv_buffer_len = 0; // 버퍼 초기화
                    client->incomplete_frame = false;
                }
                break;
            }

            case TASK_MESSAGE_INCOMPLETE_FRAME: {
                //printf("TASK_MESSAGE_INCOMPLETE_FRAME\n");
                if (process_buffer(cm, client, (char *)task.data, task.data_len) == 0) {
                    client->incomplete_frame = true;
                    // INCOMPLETE 보낸다음 클라이언트 상태 바꾼다음, 다음 버퍼(UNKNOWN_MESSAGE)를 기다린다
                }
                break;
            }

            case TASK_MESSAGE_INCOMPLETE_HTTP: {
                //printf("TASK_MESSAGE_INCOMPLETE_HTTP\n");
                if (process_buffer(cm, client, (char *)task.data, task.data_len) == 0) {
                    client->incomplete_http = true;
                    // INCOMPLETE 보낸다음 클라이언트 상태 바꾼다음, 다음 버퍼(TASK_FRAME_MESSAGE)를 기다린다
                }
                break;
            }

            case TASK_UNKNOWN_MESSAGE: {
                //printf("TASK_UNKNOWN_MESSAGE\n");
                if (client->incomplete_http == true) {
                    if (process_buffer(cm, client, (char *)task.data, task.data_len) == 0) {
                        handle_http_request(client); // HTTP 요청 처리
                    }
                    client->incomplete_http = false;
                    client->recv_buffer_len = 0;
                }
                break;
            }

            case TASK_CLIENT_CLOSE :{
                if (client != NULL) {
                    removeClient(cm, client->socket_fd);
                }

                break;
            }

            case TASK_WEBSOCKET_CLOSE : {
                if (client != NULL) {
                    printf("웹소켓 접속 종료\n");

                    // 종료 프레임 (Opcode: 0x8)
                    unsigned char close_frame[4];
                    close_frame[0] = 0x88;  // FIN bit + Opcode (0x8 for Close)
                    close_frame[1] = 0x02;  // Payload length (2 bytes for close code)

                    // 상태 코드를 네트워크 바이트 순서로 변환
                    uint16_t close_code = htons(1000);
                    memcpy(&close_frame[2], &close_code, sizeof(close_code));

                    // 종료 프레임 전송
                    if (send(client->socket_fd, close_frame, sizeof(close_frame), 0) < 0) {
                        perror("Failed to send close frame");
                    } else {
                        // printf("Close frame sent with code: %d\n", close_code);
                    }
                    removeClient(cm, client->socket_fd);
                }
                break;
            }

            default: {
                break;
            }
        }
    }

    pthread_exit(NULL);
}

int process_buffer(ClientManager *manager, Client *client, char *buffer, size_t len) {

    // 수신된 데이터를 버퍼에 추가, 버퍼 크기 검사
    if ((client->recv_buffer_len + len) < (REQUEST_BUFFER_SIZE * sizeof(char))){

        memcpy(client->recv_buffer + client->recv_buffer_len, buffer, len); // 버퍼에 남아 있는 곳에서 부터 copy
        client->recv_buffer_len += len; // 길이 갱신
        free(buffer);
        return 0;
    }

    fprintf(stderr, "클라이언트 버퍼 오버플로우 Client : %d\n", client->socket_fd);
    // 에러 처리 (연결 종료)
    removeClient(manager, client->socket_fd);
    free(buffer);
    return 1;
}

// 파일 디스크립터를 논블로킹 모드로 설정
int set_nonblocking(const int fd) {
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

//클라이언트 매니저 초기화 함수
int initClientManager(
    ClientManager* manager,
    TaskQueue *canvas_queue,
    const int port,
    const int events_size,
    const int queue_size
    ) {

    printf("클라이언트 매니저 초기화 시작\n");

    manager->port_number = port;
    manager->canvas_queue = canvas_queue;
    manager->client_count = 0;

    // linked 리스트 초기화
    manager->head = NULL;

    // 이벤트 배열 초기화
    manager->events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * events_size);
    printf("Epoll 이벤트 배열 할당 완료\n");

    // Task Queue 할당
    manager->queue = (TaskQueue *)malloc(sizeof(TaskQueue));
    init_task_queue(manager->queue, queue_size);
    printf("클라이언트 매니저 Task Queue 초기화 완료\n");

    // 서버 초기화
    // 서버 소켓 생성
    manager->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (manager->server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int optvalue=1;
    setsockopt(manager->server_socket, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    printf("서버 소켓 생성 완료\n");

    // 서버 소켓을 논블로킹 모드로 설정
    if (set_nonblocking(manager->server_socket) == -1) {
        close(manager->server_socket);
        exit(EXIT_FAILURE);
    }

    // 서버 주소 구조체 초기화
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    printf("서버 구조체 생성 완료\n");

    // 소켓 바인딩
    if (bind(manager->server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        close(manager->server_socket);
        exit(EXIT_FAILURE);
    }

    printf("서버 바인딩 완료\n");

    // 소켓 리슨
    if (listen(manager->server_socket, SOMAXCONN) == -1) {
        perror("listen failed");
        close(manager->server_socket);
        exit(EXIT_FAILURE);
    }

    printf("서버 리슨 완료\n");

    // epoll 파일 디스크립터 생성
    manager->epoll_fd = epoll_create1(0);
    if (manager->epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(manager->server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Epoll 파일 디스크립터 생성 완료\n");

    // 서버 소켓을 epoll에 등록
    manager->ev.events = EPOLLIN | EPOLLET; // 읽기 이벤트 + Edge Triggered
    manager->ev.data.fd = manager->server_socket;
    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, manager->server_socket, &manager->ev) == -1) {
        perror("epoll_ctl failed");
        close(manager->server_socket);
        close(manager->epoll_fd);
        exit(EXIT_FAILURE);
    }
    printf("서버 소켓 Epoll 등록 완료\n");

    // 스레드 생성
    const int n = pthread_create(&manager->tid, NULL, worker_thread, (void *)manager);
    if (n != 0) {
        fprintf(stderr, "클라이언트 매니저 스레드 생성 실패: %s\n", strerror(n));
        close(manager->server_socket);
        close(manager->epoll_fd);
        exit(EXIT_FAILURE);
    }
    printf("클라이언트 매니저 스레드 생성 완료\n");

    pthread_spin_init(&manager->lock, PTHREAD_PROCESS_SHARED);

    printf("클라이언트 매니저(서버) 초기화 완료 "
           "Port: %d, "
           "이벤트 버퍼 사이즈: %d, "
           "Task Queue 사이즈 %d \n"
           , port, events_size, queue_size);

    return 0;
}

// 클라이언트 추가
void addClient(ClientManager* manager) {

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    const int client_socket = accept(manager->server_socket, (struct sockaddr *)&client_addr, &client_len);

    if (client_socket == -1) {
        perror("accept failed");
        return;
    }

    // 클라이언트 소켓을 논블로킹 모드로 설정
    if (set_nonblocking(client_socket) == -1) {
        perror("client socket non blocking failed");
        close(client_socket);
        return;
    }

    // 클라이언트 소켓을 epoll에 등록
    manager->ev.events = EPOLLIN | EPOLLET; // 읽기 이벤트 + Edge Triggered
    manager->ev.data.fd = client_socket;
    if (epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, client_socket, &manager->ev) == -1) {
        perror("epoll_ctl add client failed");
        close(client_socket);
        return;
    }

    // 클라이언트 구조체 할당.
    Client* new_client = (Client*)malloc(sizeof(Client));
    if (!new_client) {
        perror("Failed to allocate memory for new client");
        epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
        close(client_socket);
        return;
    }

    // 클라이언트 구조체 작성
    new_client->socket_fd = client_socket;
    new_client->state = CONNECTION_HANDSHAKE;
    new_client->recv_buffer_len = 0;
    memset(new_client->recv_buffer, 0, sizeof(new_client->recv_buffer));
    new_client->next = NULL;
    new_client->incomplete_frame = false;
    new_client->incomplete_http = false;

    // 리스트의 맨 앞에 추가
    new_client->next = manager->head;
    manager->head = new_client;

    pthread_spin_lock(&manager->lock);
    // 현재 접속한 클라이언트 수 증가
    manager->client_count++;
    pthread_spin_unlock(&manager->lock);

    // printf("새로운 클라이언트 접속: IP = %s, Port = %d\n",
    //     inet_ntoa(client_addr.sin_addr),
    //     ntohs(client_addr.sin_port));
}

// 클라이언트 제거
int removeClient(ClientManager* manager, const int client_fd) {

    Client* current = manager->head;
    Client* prev = NULL;

    while (current != NULL) {
        if (current->socket_fd == client_fd) {
            if (prev == NULL) {
                manager->head = current->next;
            } else {
                prev->next = current->next;
            }
            epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
            close(current->socket_fd);
            free(current);
            pthread_spin_lock(&manager->lock);
            manager->client_count--;
            pthread_spin_unlock(&manager->lock);
            // printf("Client disconnected: FD %d\n", client_fd);
            return 0;
        }
        prev = current;
        current = current->next;
    }

    printf("Client with FD %d not found\n", client_fd);
    return -1;
}

// 모든 클라이언트에게 메시지 보내기
void broadcastClients(ClientManager* manager, char* message, size_t message_len) {

    char *buf = (char *)malloc(sizeof(char) * message_len);
    memcpy(buf, message, message_len);
    free(message);

    Client* current = manager->head;
    while (current != NULL && current->state == CONNECTION_OPEN) {
        // printf("broadcasting Client: %d\n", current->socket_fd);
        if (send(current->socket_fd, buf, message_len, 0) == -1) {
            perror("Broadcast");
            // 에러 나도 무시
        }
        current = current->next;
    }

    free(buf);
}

// 클라이언트 매니저 정리
void destroyClientManger(ClientManager* manager) {

    // linked list에 저장된 클라이언트들 접속 및 할당 해제
    Client* current = manager->head;
    while (current != NULL) {
        Client* temp = current;
        current = current->next;
        close(temp->socket_fd);
        free(temp);
    }
    manager->head = NULL;

    close(manager->server_socket);
    close(manager->epoll_fd);
    free(manager->events);
    free(manager);
    
}

Client* find_client(ClientManager* manager, int fd) {

    Client* current = manager->head;
    while (current != NULL) {
        if (current->socket_fd == fd) {

            return current;
        }
        current = current->next;
    }

    return NULL;
}

int get_client_count(ClientManager* manager) {

    pthread_spin_lock(&manager->lock);
    int count = manager->client_count;
    pthread_spin_unlock(&manager->lock);
    return count;
}
