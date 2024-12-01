#include "client_manager.h"

#include <http_handler.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

static void *worker_thread(void *arg) {

    pthread_t tid = pthread_self();
    printf("ClientManager Thread : %ld\n", tid);

    ClientManager *cm = (ClientManager *)arg;
    TaskQueue *queue = cm->queue;
    

    while (1) {
        Task task = pop_task(queue);
        Client *client = task.client;

        switch (task.type) {
            case TASK_NEW_CLIENT: {
                addClient(cm);
                break;
            }

            case TASK_BROADCAST: {
                broadcastClients(cm, task.data);
                break;
            }

            case TASK_HTTP_REQUEST: {               
                handle_http_request(client); // HTTP 요청 처리
                client->recv_buffer_len = 0; // 버퍼 초기화
                break;
            }

            case TASK_CLIENT_CLOSE:{
                removeClient(cm, client->socket_fd);
                epoll_ctl(cm->epoll_fd, EPOLL_CTL_DEL, client->socket_fd, NULL);
                break;
            }

            default: {
                break;
            }
        }
    }

    return NULL;
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
    const int port,
    const int events_size,
    const int queue_size

    ) {

    printf("클라이언트 매니저 초기화 시작\n");

    manager->port_number = port;

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

    pthread_mutex_init(&manager->lock, NULL);

    // 스레드 생성
    const int n = pthread_create(&manager->tid, NULL, worker_thread, (void *)manager);
    if (n != 0) {
        fprintf(stderr, "클라이언트 매니저 스레드 생성 실패: %s\n", strerror(n));
        close(manager->server_socket);
        close(manager->epoll_fd);
        exit(EXIT_FAILURE);
    }
    printf("클라이언트 매니저 스레드 생성 완료\n");

    printf("클라이언트 매니저(서버) 초기화 완료 "
           "Port: %d, "
           "이벤트 버퍼 사이즈: %d, "
           "Task Queue 사이즈 %d \n"
           , port, events_size, queue_size);

    return 0;
}

// 클라이언트 추가
void addClient(ClientManager* manager) {

    pthread_mutex_lock(&manager->lock);

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
        return;
    }

    // 클라이언트 구조체 작성
    new_client->socket_fd = client_socket;
    new_client->state = CONNECTION_HANDSHAKE;
    new_client->recv_buffer_len = 0;
    memset(new_client->recv_buffer, 0, sizeof(new_client->recv_buffer));
    new_client->next = NULL;

    // 리스트의 맨 앞에 추가
    new_client->next = manager->head;
    manager->head = new_client;

    printf("새로운 클라이언트 접속: IP = %s, Port = %d\n",
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port));

    pthread_mutex_unlock(&manager->lock);
}

// 클라이언트 제거
int removeClient(ClientManager* manager, const int client_fd) {

    pthread_mutex_lock(&manager->lock);

    Client* current = manager->head;
    Client* prev = NULL;

    while (current != NULL) {
        if (current->socket_fd == client_fd) {
            if (prev == NULL) {
                manager->head = current->next;
            } else {
                prev->next = current->next;
            }
            close(current->socket_fd);
            free(current);
            printf("Client disconnected: FD %d\n", client_fd);

            return 0;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&manager->lock);

    printf("Client with FD %d not found\n", client_fd);
    return -1;
}

// 모든 클라이언트에게 메시지 보내기
void broadcastClients(ClientManager* manager, char* message) {

    size_t len = strlen(message);
    char *buf = (char *)malloc(sizeof(char) * (len + 2));
    strcpy(buf, message);
    free(message);

    buf[len] = '\n';      // 개행 문자 추가
    buf[len + 1] = '\0';  // 문자열 종료 문자 추가

    pthread_mutex_lock(&manager->lock);

    Client* current = manager->head;
    while (current != NULL) {
        // printf("broadcasting : %s\n", buf);
        if (send(current->socket_fd, buf, len + 1, 0) == -1) {
            perror("Broadcast");
            // 에러 나도 무시
        }
        current = current->next;
    }

    pthread_mutex_unlock(&manager->lock);

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

    pthread_mutex_destroy(&manager->lock);
    close(manager->server_socket);
    close(manager->epoll_fd);
    free(manager->events);
    free(manager);
    
}

Client* find_client(ClientManager* manager, int fd) {
    
    pthread_mutex_lock(&manager->lock);

    Client* current = manager->head;
    while (current != NULL) {
        if (current->socket_fd == fd) {
            pthread_mutex_unlock(&manager->lock);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&manager->lock);
    return NULL;
}

