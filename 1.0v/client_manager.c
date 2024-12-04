#include "client_manager.h"

#include <cjson/cJSON.h>

// 클라이언트 매니저 초기화
int initClientManager(ClientManager* manager) {

    manager->head = NULL;
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        perror("Mutex initialization failed");
        return -1;
    }

    printf("Client Manager Init\n");

    return 0;
}

// 클라이언트 추가
int addClient(ClientManager* manager, const int client_fd) {

    Client* new_client = (Client*)malloc(sizeof(Client));
    if (!new_client) {
        perror("Failed to allocate memory for new client");
        return -1;
    }
    new_client->socket_fd = client_fd;
    new_client->next = NULL;

    // 뮤텍스 잠금
    if (pthread_mutex_lock(&manager->mutex) != 0) {
        perror("Mutex lock failed");
        free(new_client);
        return -1;
    }

    // 리스트의 맨 앞에 추가
    new_client->next = manager->head;
    manager->head = new_client;

    // 뮤텍스 해제
    if (pthread_mutex_unlock(&manager->mutex) != 0) {
        perror("Mutex unlock failed");
        return -1;
    }

    return 0;
}

// 클라이언트 제거
int removeClient(ClientManager* manager, const int client_fd) {
    // 뮤텍스 잠금
    if (pthread_mutex_lock(&manager->mutex) != 0) {
        perror("Mutex lock failed");
        return -1;
    }

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

            // 뮤텍스 해제
            if (pthread_mutex_unlock(&manager->mutex) != 0) {
                perror("Mutex unlock failed");
                return -1;
            }
            return 0;
        }
        prev = current;
        current = current->next;
    }

    // 뮤텍스 해제
    if (pthread_mutex_unlock(&manager->mutex) != 0) {
        perror("Mutex unlock failed");
        return -1;
    }

    printf("Client with FD %d not found\n", client_fd);
    return -1;
}

// 모든 클라이언트에게 메시지 보내기
int broadcastClients(ClientManager* manager, const char* message) {

    size_t len = strlen(message);
    char *buf = (char *)malloc(sizeof(char) * (len));
    strcpy(buf, message);;

    // 뮤텍스 잠금
    if (pthread_mutex_lock(&manager->mutex) != 0) {
        perror("Mutex lock failed");
        return -1;
    }

    Client* current = manager->head;
    while (current != NULL) {
        //printf("send : %s\n", buf);
        if (send(current->socket_fd, buf, len + 1, 0) == -1) {
            perror("broadcast");
        }
        current = current->next;
    }

    // 뮤텍스 해제
    if (pthread_mutex_unlock(&manager->mutex) != 0) {
        perror("Mutex unlock failed");
        free(buf);
        return -1;
    }

    free(buf);

    return 0;
}

// 클라이언트 매니저 정리
void cleanupClientManager(ClientManager* manager) {

    // 뮤텍스 잠금
    if (pthread_mutex_lock(&manager->mutex) != 0) {
        perror("Mutex lock failed");
        return;
    }

    // linked list에 저장된 클라이언트들 접속 및 할당 해제
    Client* current = manager->head;
    while (current != NULL) {
        Client* temp = current;
        current = current->next;
        close(temp->socket_fd);
        free(temp);
    }
    manager->head = NULL;

    // 뮤텍스 해제
    if (pthread_mutex_unlock(&manager->mutex) != 0) {
        perror("Mutex unlock failed");
    }

    // 뮤텍스 파괴
    if (pthread_mutex_destroy(&manager->mutex) != 0) {
        perror("Mutex destroy failed");
    }

    free(manager);

}
