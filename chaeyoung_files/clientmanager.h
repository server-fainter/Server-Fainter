#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H


#define MAX_CLIENTS 1024 // 최대 클라이언트 수 정의
#define BUFFER_SIZE 1024 // 버퍼 사이즈
#define PORT 8080 // 포트 번호

// 클라이언트 관리 구조체
typedef struct {
    int sockets[MAX_CLIENTS]; 
    int count;                
} ClientManager;

// 클라이언트 관리 초기화 함수
// 클라이언트 관리 구조체를 초기화합니다.
void init_clients(ClientManager *manager);

// 클라이언트 추가 함수
// 클라이언트를 배열에 추가하며, 최대 클라이언트 수를 초과하면 추가되지 않습니다.
void add_client(ClientManager *manager, int client_fd);

// 클라이언트 제거 함수
// 특정 클라이언트를 배열에서 제거합니다.
void remove_client(ClientManager *manager, int client_fd);

#endif // CLIENTMANAGER_H
