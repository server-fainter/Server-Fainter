// server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#define PORT 8080
#define WIDTH 100
#define HEIGHT 100
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100

static uint8_t canvas[WIDTH][HEIGHT];
static pthread_mutex_t canvas_mutex;

// 클라이언트 구조체
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    pthread_t thread;
    int active;
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex;

// 모든 클라이언트에게 메시지 브로드캐스트하는 함수
void broadcast_to_clients(unsigned char *message, size_t len) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            size_t frame_size = 0;
            uint8_t header[10];
            header[0] = 0x82; // FIN 비트 설정, 바이너리 프레임
            if (len <= 125) {
                header[1] = len;
                frame_size = 2;
            } else if (len <= 65535) {
                header[1] = 126;
                header[2] = (len >> 8) & 0xFF;
                header[3] = len & 0xFF;
                frame_size = 4;
            } else {
                // 큰 메시지는 처리하지 않음
                continue;
            }

            // 헤더 전송
            ssize_t sent_bytes = send(clients[i].socket_fd, header, frame_size, 0);
            if (sent_bytes < 0) {
                perror("Failed to send to client");
                clients[i].active = 0;
                close(clients[i].socket_fd);
                continue;
            }

            // 페이로드 전송
            sent_bytes = send(clients[i].socket_fd, message, len, 0);
            if (sent_bytes < 0) {
                perror("Failed to send to client");
                clients[i].active = 0;
                close(clients[i].socket_fd);
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Base64 인코딩 함수 구현
void base64_encode(const unsigned char *input, int length, char *output, int output_size) {
    int encoded_length = EVP_EncodeBlock((unsigned char *)output, input, length);
    if (encoded_length < 0 || encoded_length >= output_size) {
        fprintf(stderr, "Base64 encoding failed\n");
    }
    output[encoded_length] = '\0';
}

// SHA-1 해시 함수

void sha1_hash(const char *input, size_t len, unsigned char *output) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (mdctx == NULL) {
        fprintf(stderr, "EVP_MD_CTX_new failed\n");
        return;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL) != 1) {
        fprintf(stderr, "EVP_DigestInit_ex failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    if (EVP_DigestUpdate(mdctx, input, len) != 1) {
        fprintf(stderr, "EVP_DigestUpdate failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    unsigned int md_len;
    if (EVP_DigestFinal_ex(mdctx, output, &md_len) != 1) {
        fprintf(stderr, "EVP_DigestFinal_ex failed\n");
        EVP_MD_CTX_free(mdctx);
        return;
    }

    EVP_MD_CTX_free(mdctx);
}


// WebSocket 핸드셰이크 처리 함수
int websocket_handshake(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0); // 널 종결자 위해 -1
    if (received <= 0) {
        return -1;
    }

    buffer[received] = '\0';

    // 수신한 요청을 출력하여 확인
    printf("Received request:\n%s\n", buffer);

    // 업그레이드 요청인지 확인
    if (strstr(buffer, "Upgrade: websocket") == NULL) {
        // WebSocket 업그레이드 요청이 아님
        printf("Upgrade header not found.\n");
        return -1;
    }

    // Sec-WebSocket-Key 추출
    char *key_header = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_header) {
        printf("Sec-WebSocket-Key header not found.\n");
        return -1;
    }

    key_header += strlen("Sec-WebSocket-Key: ");
    char *key_end = strstr(key_header, "\r\n");
    if (!key_end) {
        printf("Sec-WebSocket-Key header end not found.\n");
        return -1;
    }

    char client_key[256];
    size_t key_len = key_end - key_header;
    if (key_len >= sizeof(client_key)) {
        key_len = sizeof(client_key) - 1;
    }
    strncpy(client_key, key_header, key_len);
    client_key[key_len] = '\0';

    // GUID와 합친 후 SHA-1 해싱 및 Base64 인코딩
    const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char key_guid[300]; // 버퍼 크기 증가
    int n = snprintf(key_guid, sizeof(key_guid), "%s%s", client_key, guid);
    if (n < 0 || n >= sizeof(key_guid)) {
        fprintf(stderr, "key_guid buffer overflow\n");
        return -1;
    }

    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    sha1_hash(key_guid, strlen(key_guid), sha1_result);

    char accept_key[256];
    base64_encode(sha1_result, SHA_DIGEST_LENGTH, accept_key, sizeof(accept_key));

    // 응답 헤더 생성 및 전송
    char response[512];
    n = snprintf(response, sizeof(response),
                 "HTTP/1.1 101 Switching Protocols\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Accept: %s\r\n\r\n",
                 accept_key);
    if (n < 0 || n >= sizeof(response)) {
        fprintf(stderr, "Response buffer overflow\n");
        return -1;
    }

    if (send(client_fd, response, strlen(response), 0) < 0) {
        return -1;
    }

    return 0;
}


// WebSocket 프레임을 읽는 함수
ssize_t websocket_read(int client_fd, uint8_t *buffer, size_t buffer_size) {
    uint8_t header[2];
    ssize_t received = recv(client_fd, header, 2, 0);
    if (received <= 0) {
        return -1;
    }

    uint8_t masked = header[1] & 0x80;
    uint64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext_payload_len[2];
        recv(client_fd, ext_payload_len, 2, 0);
        payload_len = (ext_payload_len[0] << 8) | ext_payload_len[1];
    } else if (payload_len == 127) {
        // 큰 메시지는 처리하지 않음
        return -1;
    }

    uint8_t masking_key[4];
    if (masked) {
        recv(client_fd, masking_key, 4, 0);
    }

    if (payload_len > buffer_size) {
        // 버퍼 크기보다 큰 경우 처리하지 않음
        return -1;
    }

    ssize_t total_received = 0;
    while (total_received < payload_len) {
        ssize_t r = recv(client_fd, buffer + total_received, payload_len - total_received, 0);
        if (r <= 0) {
            return -1;
        }
        total_received += r;
    }

    if (masked) {
        for (uint64_t i = 0; i < payload_len; i++) {
            buffer[i] ^= masking_key[i % 4];
        }
    }

    return payload_len;
}

// 클라이언트 처리 함수
void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    uint8_t buffer[BUFFER_SIZE];
    unsigned char *init_msg = NULL;
    unsigned char *frame = NULL;

    printf("Client connected: %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));

    // WebSocket 핸드셰이크 처리
    if (websocket_handshake(cli->socket_fd) < 0) {
        printf("Handshake failed\n");
        goto cleanup;
    }

    // 초기 캔버스 상태 전송
    pthread_mutex_lock(&canvas_mutex);
    size_t init_len = WIDTH * HEIGHT * 3;
    init_msg = malloc(init_len);
    if (!init_msg) {
        perror("Failed to allocate memory for init_msg");
        pthread_mutex_unlock(&canvas_mutex);
        goto cleanup;
    }
    size_t index = 0;
    for (uint8_t y = 0; y < HEIGHT; y++) {
        for (uint8_t x = 0; x < WIDTH; x++) {
            init_msg[index++] = x;
            init_msg[index++] = y;
            init_msg[index++] = canvas[x][y];
        }
    }
    pthread_mutex_unlock(&canvas_mutex);

    // WebSocket 프레임 작성 및 전송
   size_t frame_size = 0;
    size_t max_frame_size = 10 + init_len;
    frame = malloc(max_frame_size);
    if (!frame) {
        perror("Failed to allocate memory for frame");
        goto cleanup;
    }

    frame[0] = 0x82; // FIN 비트 설정, 바이너리 프레임
    if (init_len <= 125) {
        frame[1] = init_len;
        frame_size = 2;
    } else if (init_len <= 65535) {
        frame[1] = 126;
        frame[2] = (init_len >> 8) & 0xFF;
        frame[3] = init_len & 0xFF;
        frame_size = 4;
    } else {
        goto cleanup;
    }

    memcpy(&frame[frame_size], init_msg, init_len);
    frame_size += init_len;

    if (send(cli->socket_fd, frame, frame_size, 0) < 0) {
        perror("Failed to send initial canvas to client");
        goto cleanup;
    }
    free(init_msg);

    // 클라이언트로부터 데이터 수신
    while (1) {
        ssize_t payload_len = websocket_read(cli->socket_fd, buffer, sizeof(buffer));
        if (payload_len <= 0) {
            break;
        }

        // 수신된 데이터 처리
        ssize_t i = 0;
        while (i + 2 < payload_len) { // 3바이트씩 처리
            uint8_t x = buffer[i];
            uint8_t y = buffer[i + 1];
            uint8_t color = buffer[i + 2];
            i += 3;

            if (x < WIDTH && y < HEIGHT) {
                pthread_mutex_lock(&canvas_mutex);
                canvas[x][y] = color;
                pthread_mutex_unlock(&canvas_mutex);

                // 변경 사항을 모든 클라이언트에게 브로드캐스트
                unsigned char msg[3] = { x, y, color };
                broadcast_to_clients(msg, 3);
            }
        }
    }

    printf("Client disconnected: %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));

cleanup:
    if (init_msg) free(init_msg);
    if (frame) free(frame);
    close(cli->socket_fd);
    pthread_mutex_lock(&clients_mutex);
    cli->active = 0;
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

// 서버 설정 함수
int setup_server(int port) {
    int server_fd;
    struct sockaddr_in server_addr;

    // TCP 소켓 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }

    // 소켓 옵션 설정 (주소 재사용)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    // 서버 주소 구성
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // 소켓을 포트에 바인딩
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }

    // 듣기 시작
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int main(void) {
    int server_fd;
    printf("Server is running on port %d\n", PORT);

    pthread_mutex_init(&canvas_mutex, NULL);
    pthread_mutex_init(&clients_mutex, NULL);
    memset(canvas, 0, sizeof(canvas));
    memset(clients, 0, sizeof(clients));

    server_fd = setup_server(PORT);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to set up server\n");
        return -1;
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int new_socket;

        // 새로운 클라이언트 수락
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // 새로운 클라이언트를 위한 빈 슬롯 찾기
        pthread_mutex_lock(&clients_mutex);
        int idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                idx = i;
                break;
            }
        }

        if (idx != -1) {
            clients[idx].socket_fd = new_socket;
            clients[idx].address = client_addr;
            clients[idx].active = 1;

            // 새로운 클라이언트를 처리할 스레드 생성
            if (pthread_create(&clients[idx].thread, NULL, handle_client, &clients[idx]) != 0) {
                perror("Thread creation failed");
                clients[idx].active = 0;
                close(new_socket);
            } else {
                pthread_detach(clients[idx].thread); // 스레드 리소스 자동 해제
            }
        } else {
            // 새로운 클라이언트를 위한 슬롯이 없음
            printf("Maximum clients connected. Connection refused.\n");
            close(new_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // 정리
    close(server_fd);
    pthread_mutex_destroy(&canvas_mutex);
    pthread_mutex_destroy(&clients_mutex);

    return 0;
}
