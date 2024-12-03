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
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>


#define PORT 8080
#define WIDTH 500
#define HEIGHT 500
#define BUFFER_SIZE 8192
#define MAX_CLIENTS 10000
#define MAX_EVENTS 10000
#define CANVAS_FILE "canvas.dat"
#define SAVE_INTERVAL 60 // 60초마다 저장

#define MSG_TYPE_CANVAS_UPDATE 1
#define MSG_TYPE_CLIENT_COUNT  2
FILE *log_file;
pthread_mutex_t log_file_mutex;

static uint8_t canvas[WIDTH][HEIGHT];
static pthread_mutex_t canvas_mutex;

unsigned clientCnt=0;

// 클라이언트 구조체
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    int active;
    int handshake_done;
    uint8_t buffer[BUFFER_SIZE];
    size_t buffer_offset;
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex;

// 캔버스 저장 함수
int save_canvas_to_file(const char *filename) {
    const char *temp_filename = "canvas.tmp";
    FILE *fp = fopen(temp_filename, "wb");
    if (!fp) {
        perror("Failed to open temporary canvas file for writing");
        return -1;
    }

    pthread_mutex_lock(&canvas_mutex);
    size_t written = fwrite(canvas, sizeof(uint8_t), WIDTH * HEIGHT, fp);
    pthread_mutex_unlock(&canvas_mutex);

    fclose(fp);

    if (written != WIDTH * HEIGHT) {
        fprintf(stderr, "Failed to write complete canvas data to temporary file\n");
        return -1;
    }

    // 임시 파일을 실제 파일로 교체 , 데이터 손실 방지
    if (rename(temp_filename, filename) != 0) {
        perror("Failed to rename temporary canvas file");
        return -1;
    }

    return 0;
}
// 모든 클라이언트의 동접자 수를 브로트 캐스트 하는 함수
void broadcast_client_count(unsigned int clientCnt) {
    pthread_mutex_lock(&clients_mutex);

    unsigned char message[5]; 
    message[0] = 0x02; // 메시지 타입: 동접자 수
    message[1] = (clientCnt >> 24) & 0xFF;
    message[2] = (clientCnt >> 16) & 0xFF;
    message[3] = (clientCnt >> 8) & 0xFF;
    message[4] = clientCnt & 0xFF;

    size_t len = sizeof(message);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].handshake_done) {
            int client_fd = clients[i].socket_fd;
            // WebSocket 프레임 작성

            uint8_t header[14];
            size_t header_size = 0;
            header[0] = 0x82; // FIN 비트 설정, 바이너리 프레임

            if (len <= 125) {
                header[1] = len;
                header_size = 2;
            } else if (len <= 65535) {
                header[1] = 126;
                header[2] = (len >> 8) & 0xFF;
                header[3] = len & 0xFF;
                header_size = 4;
            } else {
                continue;
            }

            struct iovec iov[2];
            iov[0].iov_base = header;
            iov[0].iov_len = header_size;
            iov[1].iov_base = message;
            iov[1].iov_len = len;

            ssize_t sent_bytes = writev(client_fd, iov, 2);
            if (sent_bytes < 0) {
                perror("Failed to send client count");
                clients[i].active = 0;
                close(clients[i].socket_fd);
                clientCnt--;
                printf("동접자 수:%d(나감)\n",clientCnt);
                broadcast_client_count(clientCnt);
                printf("Closed connection with %s:%d due to send failure\n",
                       inet_ntoa(clients[i].address.sin_addr),
                       ntohs(clients[i].address.sin_port));
                continue;
            }

            printf("Broadcasted client count (%u) to %s:%d\n", clientCnt,
                   inet_ntoa(clients[i].address.sin_addr),
                   ntohs(clients[i].address.sin_port));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// 캔버스 로드 함수
int load_canvas_from_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        // 파일이 없으면 초기화된 상태를 유지
        printf("Canvas file not found. Initializing with default values.\n");
        return 0;
    }

    size_t read = fread(canvas, sizeof(uint8_t), WIDTH * HEIGHT, fp);
    fclose(fp);

    if (read != WIDTH * HEIGHT) {
        fprintf(stderr, "Failed to read complete canvas data. Initializing with default values.\n");
        memset(canvas, 0, sizeof(canvas));
        return -1;
    }

    printf("Canvas loaded from file successfully.\n");
    return 0;
}

// 주기적으로 캔버스 저장하는 함수
void *save_canvas_periodically(void *arg) {
    while (1) {
        sleep(SAVE_INTERVAL);
        if (save_canvas_to_file(CANVAS_FILE) == 0) {
            printf("Canvas saved successfully.\n");
        } 
        else {
            printf("Failed to save canvas.\n");
        }
    }
    return NULL;
}

// 모든 클라이언트에게 메시지 브로드캐스트하는 함수
void broadcast_to_clients(unsigned char *pixel_updates, size_t len) {
    pthread_mutex_lock(&clients_mutex);
    struct timeval t1, t2;
    double elapsedTime;
    gettimeofday(&t1, NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].handshake_done) {
            int client_fd = clients[i].socket_fd;

            // 메시지 타입 0x01 추가
            unsigned char *message = malloc(len + 1);
            if (!message) {
                perror("Failed to allocate memory for message");
                continue;
            }
            message[0] = 0x01;
            memcpy(message + 1, pixel_updates, len);
            size_t total_len = len + 1;

            // WebSocket 프레임 작성
            uint8_t header[14];
            size_t header_size = 0;
            header[0] = 0x82; // FIN 비트 설정, 바이너리 프레임

            if (total_len <= 125) {
                header[1] = total_len;
                header_size = 2;
            } else if (total_len <= 65535) {
                header[1] = 126;
                header[2] = (total_len >> 8) & 0xFF;
                header[3] = total_len & 0xFF;
                header_size = 4;
            } else {
                header[1] = 127;
                for (int j = 0; j < 8; j++) {
                    header[2 + j] = (total_len >> (56 - 8 * j)) & 0xFF;
                }
                header_size = 10;
            }

            struct iovec iov[2];
            iov[0].iov_base = header;
            iov[0].iov_len = header_size;
            iov[1].iov_base = message;
            iov[1].iov_len = total_len;

            ssize_t sent_bytes = writev(client_fd, iov, 2);
            if (sent_bytes < 0) {
                perror("Failed to send to client");
                clients[i].active = 0;
                close(clients[i].socket_fd);
                clientCnt--;
                printf("동접자 수:%d(나감)\n",clientCnt);
                broadcast_client_count(clientCnt);
                printf("Closed connection with %s:%d due to send failure\n",
                       inet_ntoa(clients[i].address.sin_addr),
                       ntohs(clients[i].address.sin_port));
                free(message);
                continue;
            }

            printf("Broadcasted %zu bytes to %s:%d\n", total_len,
                   inet_ntoa(clients[i].address.sin_addr),
                   ntohs(clients[i].address.sin_port));
            free(message);
        }
    }
    
    pthread_mutex_unlock(&clients_mutex);
    gettimeofday(&t2, NULL);
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      
    elapsedTime += ((t2.tv_usec - t1.tv_usec) / 1000.0); 
    printf("broadcast_to_clients 수행시간: %f ms\n", elapsedTime);

    pthread_mutex_lock(&log_file_mutex);
    if (log_file) {
        // 현재 시간 가져오기
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[26];
        strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

        // 클라이언트 수 가져오기 (스레드 안전하게)
        pthread_mutex_lock(&clients_mutex);
        unsigned int current_client_count = clientCnt;
        pthread_mutex_unlock(&clients_mutex);

        // CSV 형식으로 기록: timestamp,client_count,elapsed_time_ms
        fprintf(log_file, "%s,%u,%.3f\n", timestamp, current_client_count, elapsedTime);
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_file_mutex);
}


// Base64 인코딩 함수 구현, handshake 함수에서 사용
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

// 소켓을 논블로킹 모드로 설정하는 함수
void set_nonblocking(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) flags = 0;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
    }
}

// WebSocket 핸드셰이크 처리 함수
int websocket_handshake(client_t *cli) {
    char buffer[BUFFER_SIZE];
    ssize_t received = recv(cli->socket_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return -1;
    }

    buffer[received] = '\0';

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
    char key_guid[300];
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

    ssize_t sent = send(cli->socket_fd, response, strlen(response), 0);
    if (sent < 0) {
        perror("send failed");
        clientCnt--;
        printf("동접자 수:%d(나감)\n",clientCnt);
        broadcast_client_count(clientCnt);
        return -1;
    }

    printf("Handshake completed with %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));

    return 0;
}

// WebSocket 프레임을 처리하는 함수
ssize_t process_websocket_frame(client_t *cli) {
    uint8_t *buffer = cli->buffer;
    size_t buffer_len = cli->buffer_offset;
    size_t payload_len = 0;
    size_t header_len = 2;
    if (buffer_len < 2) {
        // 헤더가 완전하지 않음
        return 0;
    }

    uint8_t opcode = buffer[0] & 0x0F;
    uint8_t masked = (buffer[1] & 0x80) != 0;
    payload_len = buffer[1] & 0x7F;
    if (payload_len == 126) { 
        header_len += 2;
        if (buffer_len < header_len) return 0;
        payload_len = (buffer[2] << 8) | buffer[3]; // big-endian
    } 
    else if (payload_len == 127) {
        header_len += 8;
        if (buffer_len < header_len) return 0;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | buffer[2 + i];
        }
    }

    size_t masking_key_offset = header_len; // 2
    if (masked) {
        header_len += 4;
    }

    size_t total_frame_len = header_len + payload_len;
    if (buffer_len < total_frame_len) {
        // 전체 프레임이 아직 도착하지 않음
        return 0; 
    }

    uint8_t *payload_data = buffer + header_len;

    if (masked) {
        uint8_t *masking_key = buffer + masking_key_offset;
        for (size_t i = 0; i < payload_len; i++) {
            payload_data[i] ^= masking_key[i % 4];
        }
    }

    // opcode에 따라 처리
    if (opcode == 0x8) {
        // 연결 종료 프레임
        printf("Client disconnected: %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
        close(cli->socket_fd);
        cli->active = 0;
        clientCnt--;
        printf("동접자 수:%d(나감)\n",clientCnt);
        broadcast_client_count(clientCnt);
        return total_frame_len;
    } 
    else if (opcode == 0x2) {
        // 바이너리 메시지 처리
        printf("Received binary message from %s:%d - Payload Length: %zu\n", 
               inet_ntoa(cli->address.sin_addr), 
               ntohs(cli->address.sin_port), 
               payload_len);
        size_t i = 0;
        while (i + 5 <= payload_len) { // 5바이트씩 처리 (x: 2바이트, y: 2바이트, color: 1바이트)
            uint16_t x = payload_data[i] | (payload_data[i + 1] << 8); 
            uint16_t y = payload_data[i + 2] | (payload_data[i + 3] << 8);
            uint8_t color = payload_data[i + 4];
            i += 5;

            if (x < WIDTH && y < HEIGHT) {
                pthread_mutex_lock(&canvas_mutex);
                canvas[x][y] = color;
                pthread_mutex_unlock(&canvas_mutex);

                // 변경 사항을 모든 클라이언트에게 브로드캐스트
                unsigned char msg[5];
                msg[0] = x & 0xFF;
                msg[1] = (x >> 8) & 0xFF;
                msg[2] = y & 0xFF;
                msg[3] = (y >> 8) & 0xFF;
                msg[4] = color;
                broadcast_to_clients(msg, 5);

                printf("Updated pixel (%d, %d) to color %d\n", x, y, color);
            }
        }
    }

    return total_frame_len;
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

    // 소켓 옵션 설정 (포트 재사용)
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
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
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

void cleanup(int signum) {
    // 로그 파일 닫기
    pthread_mutex_lock(&log_file_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_file_mutex);

    // 기타 자원 해제
    // 필요한 경우 다른 뮤텍스 파괴, 소켓 닫기 등 수행

    exit(0);
}

int main(void) {
    int server_fd, epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];
    pthread_t save_thread;
    pthread_mutex_init(&log_file_mutex, NULL);
    log_file = fopen("broadcast_log.csv", "w");
    if (!log_file) {
        perror("Failed to open log file");
        return -1;
    }
    // CSV 헤더 추가
    fprintf(log_file, "timestamp,client_count,elapsed_time_ms\n");
    fflush(log_file);

    // SIGINT 시그널 처리기 설정
    signal(SIGINT, cleanup);
    

    printf("Server is running on port %d\n", PORT);

    pthread_mutex_init(&canvas_mutex, NULL);
    pthread_mutex_init(&clients_mutex, NULL);
    memset(canvas, 29, sizeof(canvas));
    memset(clients, 0, sizeof(clients));

    // 서버 시작 전에 캔버스 데이터 로드
    if (load_canvas_from_file(CANVAS_FILE) != 0) {
        printf("Starting with default canvas.\n");
    }

    server_fd = setup_server(PORT);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to set up server\n");
        return -1;
    }

    // 서버 소켓을 논블로킹 모드로 설정
    set_nonblocking(server_fd);

    // epoll 인스턴스 생성
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(server_fd);
        return -1;
    }

    // 서버 소켓을 epoll에 추가
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl: server_fd");
        close(server_fd);
        close(epoll_fd);
        return -1;
    }

    // 캔버스 저장을 주기적으로 수행하는 스레드 생성
    if (pthread_create(&save_thread, NULL, save_canvas_periodically, NULL) != 0) {
        perror("Turn off the server, Failed to create save thread");
        // 계속 진행하되, 저장이 되지 않을 것임을 경고
    }

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {
                // 새로운 연결 수락
                while (1) {
                    int client_fd;
                    struct sockaddr_in client_addr;
                    socklen_t addrlen = sizeof(client_addr);

                    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 더 이상 수락할 클라이언트가 없음
                            break;
                        } 
                        else {
                            perror("accept");
                            break;
                        }
                    }

                    // 클라이언트 소켓을 논블로킹 모드로 설정
                    set_nonblocking(client_fd);

                    // 새로운 클라이언트를 위한 빈 슬롯 찾기
                    pthread_mutex_lock(&clients_mutex);
                    int idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (!clients[j].active) {
                            idx = j;
                            break;
                        }
                    }

                    if (idx != -1) {
                        clients[idx].socket_fd = client_fd;
                        clients[idx].address = client_addr;
                        clients[idx].active = 1;
                        clients[idx].handshake_done = 0;
                        clients[idx].buffer_offset = 0;

                        // 클라이언트 소켓을 epoll에 추가
                        event.events = EPOLLIN | EPOLLET;
                        event.data.fd = client_fd;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                            perror("epoll_ctl: client_fd");
                            close(client_fd);
                            clientCnt--;
                            printf("동접자 수:%d(나감)\n",clientCnt);
                            broadcast_client_count(clientCnt);
                            clients[idx].active = 0;
                        } 
                        else {
                            printf("Client connected: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                        }
                    } 
                    else {
                        // 새로운 클라이언트를 위한 슬롯이 없음
                        printf("Maximum clients connected. Connection refused.\n");
                        close(client_fd);
                    }
                    pthread_mutex_unlock(&clients_mutex);
                }
            } 
            else {
                // 클라이언트 이벤트 처리
                int client_fd = events[i].data.fd;
                client_t *cli = NULL;

                pthread_mutex_lock(&clients_mutex);
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].active && clients[j].socket_fd == client_fd) {
                        cli = &clients[j];
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);

                if (cli == NULL) {
                    // 알 수 없는 클라이언트
                    close(client_fd);
                    continue;
                }

                if (events[i].events & EPOLLIN) { 
                    if (!cli->handshake_done) {
                        // 핸드셰이크 수행
                        if (websocket_handshake(cli) == 0) {
                            pthread_mutex_lock(&canvas_mutex);
                            cli->handshake_done = 1;
                            size_t init_len = WIDTH * HEIGHT * 5;
                            unsigned char *init_msg = malloc(init_len + 1); // +1 for message type
                            if (!init_msg) {
                              perror("Failed to allocate memory for init_msg");
                              // Handle error
                              }
                              init_msg[0] = 0x01; // 메시지 타입: 픽셀 업데이트
                              size_t index = 1;
                              for (uint16_t y = 0; y < HEIGHT; y++) {
                                for (uint16_t x = 0; x < WIDTH; x++) {
                                  init_msg[index++] = x & 0xFF;
                                  init_msg[index++] = (x >> 8) & 0xFF;
                                  init_msg[index++] = y & 0xFF;
                                  init_msg[index++] = (y >> 8) & 0xFF;
                                  init_msg[index++] = canvas[x][y];
                                }
                              }
                            pthread_mutex_unlock(&canvas_mutex);
                        

                            // WebSocket 프레임 작성 및 전송
                            uint8_t header[14];
                            size_t header_size = 0;
                            header[0] = 0x82; // FIN 비트 설정, 바이너리 프레임

                            if (init_len <= 125) {
                                header[1] = init_len;
                                header_size = 2;
                            }
                            else if (init_len <= 65535) {
                                header[1] = 126;
                                header[2] = (init_len >> 8) & 0xFF;
                                header[3] = init_len & 0xFF;
                                header_size = 4;
                            } 
                            else {
                                header[1] = 127;
                                for (int j = 0; j < 8; j++) {
                                    header[2 + j] = (init_len >> (56 - 8*j)) & 0xFF;
                                }
                                header_size = 10;
                            }

                            struct iovec iov[2];
                            iov[0].iov_base = header;
                            iov[0].iov_len = header_size;
                            iov[1].iov_base = init_msg;
                            iov[1].iov_len = init_len;

                            ssize_t sent_bytes = writev(cli->socket_fd, iov, 2);
                            if (sent_bytes < 0) {
                                perror("Failed to send initial canvas to client");
                                free(init_msg);
                                close(cli->socket_fd);
                                cli->active = 0;
                                clientCnt--;
                                printf("동접자 수:%d(나감)\n",clientCnt);
                                broadcast_client_count(clientCnt);
                                continue;
                            }

                            printf("Initial canvas sent to %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                            clientCnt++;
                            printf("동접자 수:%d(들어옴)\n",clientCnt);
                            broadcast_client_count(clientCnt);
                            free(init_msg);
                        } 
                        else {
                            // 핸드셰이크 실패
                            printf("Handshake failed with %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                            close(cli->socket_fd);
                            broadcast_client_count(clientCnt);
                        }
                    } 
                    else {
                        // 클라이언트 데이터 처리
                        ssize_t bytes_read;
                        while ((bytes_read = recv(cli->socket_fd, cli->buffer + cli->buffer_offset, BUFFER_SIZE - cli->buffer_offset, 0)) > 0) {
                            cli->buffer_offset += bytes_read;

                            // 버퍼에서 완전한 프레임을 처리
                            while (1) {
                                ssize_t frame_len = process_websocket_frame(cli);
                                if (frame_len > 0) {
                                    // 처리된 프레임을 버퍼에서 제거
                                    memmove(cli->buffer, cli->buffer + frame_len, cli->buffer_offset - frame_len);
                                    cli->buffer_offset -= frame_len;
                                } 
                                else if (frame_len == 0) {
                                    // 더 이상 처리할 프레임이 없음
                                    break;
                                } 
                                else {
                                    // 오류 발생
                                    printf("Error processing frame for %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                                    close(cli->socket_fd);
                                    cli->active = 0;
                                    clientCnt--;
                                    printf("동접자 수:%d(나감)\n",clientCnt);
                                    broadcast_client_count(clientCnt);
                                    break;
                                }
                            }
                        }

                        if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                            // 오류 발생, 다른데와 중복처리해서 우선 뺌
                        } 
                        else if (bytes_read == 0) {
                            // 클라이언트 연결 종료
                            printf("Client disconnected: %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                            close(cli->socket_fd);
                            cli->active = 0;clientCnt--;
                            printf("동접자 수:%d(나감)\n",clientCnt);
                            broadcast_client_count(clientCnt);
                        }
                        
                    }
                }
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    // 클라이언트 연결 종료 또는 오류 발생
                    printf("Client disconnected (EPOLLHUP/EPOLLERR): %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                    close(cli->socket_fd);
                    cli->active = 0;
                    clientCnt--;
                    printf("동접자 수:%d(나감)\n",clientCnt);
                    broadcast_client_count(clientCnt);
                }
            }
        }
    }

    // 루프 종료 시 캔버스 저장, while루프를 탈출하는 방법을 넣어야함..
        
    if (save_canvas_to_file(CANVAS_FILE) == 0) {
        printf("Canvas saved successfully on shutdown.\n");
    } 
    else {
        printf("Failed to save canvas on shutdown.\n");
    }

    // 저장 스레드 종료 대기 (실제로는 이 지점에 도달하지 않음)
    pthread_cancel(save_thread);
    pthread_join(save_thread, NULL);
    // 정리
    close(server_fd);
    close(epoll_fd);
    pthread_mutex_destroy(&canvas_mutex);
    pthread_mutex_destroy(&clients_mutex);

    return 0;
}