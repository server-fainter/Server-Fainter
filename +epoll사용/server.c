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
#include <sys/uio.h> // writev 함수 사용을 위한 헤더 추가

#define PORT 8080
#define WIDTH 100
#define HEIGHT 100
#define BUFFER_SIZE 8192
#define MAX_CLIENTS 1000
#define MAX_EVENTS 1000

#define MSG_TYPE_CANVAS_UPDATE 1
#define MSG_TYPE_CLIENT_COUNT  2

static uint8_t canvas[WIDTH][HEIGHT];
static pthread_mutex_t canvas_mutex;

// 클라이언트 구조체
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    int active;
    int handshake_done;
    uint8_t buffer[BUFFER_SIZE];
    size_t buffer_offset;
} client_t;

unsigned clientCnt=0;
client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex;

// 모든 클라이언트에게 메시지 브로드캐스트하는 함수
void broadcast_canvas(unsigned char *message, size_t len) {
    pthread_mutex_lock(&clients_mutex);

    // 메시지 타입 필드를 추가한 새로운 버퍼 생성
    unsigned char new_message[len + 1];
    new_message[0] = 0x01; // 메시지 타입: 캔버스 업데이트
    memcpy(new_message + 1, message, len);

    size_t new_len = len + 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].handshake_done) {
            int client_fd = clients[i].socket_fd;
            uint8_t header[10];
            size_t header_size = 0;
            header[0] = 0x82;

            if (new_len <= 125) {
                header[1] = new_len;
                header_size = 2;
            } else if (new_len <= 65535) {
                header[1] = 126;
                header[2] = (new_len >> 8) & 0xFF;
                header[3] = new_len & 0xFF;
                header_size = 4;
            } else {
                continue;
            }
            //iovec구조체와 writev를 통해 헤더 메세지 한번에 시스템 호출로 전송
            // 지정된 소켓에 대해 iov배열에 있는 두개의 버퍼를 한번에 보냄
            struct iovec iov[2];
            iov[0].iov_base = header;
            iov[0].iov_len = header_size;
            iov[1].iov_base = new_message;
            iov[1].iov_len = new_len;

            ssize_t sent_bytes = writev(client_fd, iov, 2);
            if (sent_bytes < 0) {
                perror("Failed to send to client");
                clients[i].active = 0;
                close(clients[i].socket_fd);
                printf("Closed connection with %s:%d due to send failure\n",
                       inet_ntoa(clients[i].address.sin_addr),
                       ntohs(clients[i].address.sin_port));
                continue;
            }

            printf("Broadcasted %zu bytes to %s:%d\n", len,
                   inet_ntoa(clients[i].address.sin_addr),
                   ntohs(clients[i].address.sin_port));
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// 모든 클라이언트의 동접자 수를 브로트 캐스트 하는 함수
void broadcast_client_count(unsigned int clientCnt) {
    pthread_mutex_lock(&clients_mutex);

    // unsigned int clientCnt를 unsigned char로 변환
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
            uint8_t header[10];
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
    char *key_header = strstr(buffer, "Sec-WebSocket-Key: "); //key의 시작주소값
    if (!key_header) {
        printf("Sec-WebSocket-Key header not found.\n");
        return -1;
    }

    key_header += strlen("Sec-WebSocket-Key: "); //key의 시작주소값에 key의 길이만큼 더함 -> 키값의 시작주소값
    char *key_end = strstr(key_header, "\r\n"); //키의 끝주소값
    if (!key_end) {
        printf("Sec-WebSocket-Key header end not found.\n");
        return -1;
    }

    char client_key[256];
    size_t key_len = key_end - key_header; //키의 길이
    if (key_len >= sizeof(client_key)) { //버퍼오버플로우 방지
        key_len = sizeof(client_key) - 1; //버퍼의 마지막에 널문자를 넣어줘야 하기 때문에 -1, 잘못된 헤어가 왔을 때.
    }
    strncpy(client_key, key_header, key_len);
    client_key[key_len] = '\0';

    // GUID와 합친 후 SHA-1 해싱 및 Base64 인코딩
    const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char key_guid[300]; // 버퍼 크기 증가, key와 guid를 합친 후 해싱하기 때문에
    int n = snprintf(key_guid, sizeof(key_guid), "%s%s", client_key, guid); //key와 guid를 합침
    if (n < 0 || n >= sizeof(key_guid)) {
        fprintf(stderr, "key_guid buffer overflow\n");
        return -1;
    }

    unsigned char sha1_result[SHA_DIGEST_LENGTH]; //SHA-1 해시 결과를 저장할 버퍼
    sha1_hash(key_guid, strlen(key_guid), sha1_result); //SHA-1 해싱

    char accept_key[256];
    base64_encode(sha1_result, SHA_DIGEST_LENGTH, accept_key, sizeof(accept_key)); //해싱된 결과를 base64로 인코딩

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
        return -1;
    }

    printf("Handshake completed with %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
    
    return 0;
}

// WebSocket 프레임을 처리하는 함수
/*
[웹소켓 프레임 구조]
FIN(opcode)+payloadlength(masking여부)+payload
*/
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

    size_t masking_key_offset = header_len; // 마스킹 키의 시작 위치
    if (masked) {
        header_len += 4; // 마스킹 키 길이가 4바이트이므로 헤더 길이에 4를 더함
    }

    size_t total_frame_len = header_len + payload_len;
    if (buffer_len < total_frame_len) {
        // 전체 프레임이 아직 도착하지 않음
        return 0; 
    }

    uint8_t *payload_data = buffer + header_len; // 페이로드 데이터의 시작 위치

    if (masked) {
        uint8_t *masking_key = buffer + masking_key_offset;
        for (size_t i = 0; i < payload_len; i++) {
            payload_data[i] ^= masking_key[i % 4]; // 마스킹 키로 XOR 연산
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
    } else if (opcode == 0x2) {
        // 바이너리 메시지 처리
        size_t i = 0;
        while (i + 2 < payload_len) { // 3바이트씩 처리
            uint8_t x = payload_data[i];
            uint8_t y = payload_data[i + 1];
            uint8_t color =payload_data[i + 2];
            i += 3;

            if (x < WIDTH && y < HEIGHT) {
                pthread_mutex_lock(&canvas_mutex);
                canvas[x][y] = color;
                pthread_mutex_unlock(&canvas_mutex);

                // 변경 사항을 모든 클라이언트에게 브로드캐스트
                unsigned char msg[3] = { x, y, color };
                broadcast_canvas(msg, 3);

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

int main(void) {
    int server_fd, epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    printf("Server is running on port %d\n", PORT);

    pthread_mutex_init(&canvas_mutex, NULL);
    pthread_mutex_init(&clients_mutex, NULL);
    //기본 화면색 29번째 팔레트 값: 하양
    memset(canvas,29, sizeof(canvas));
    memset(clients, 0, sizeof(clients));

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
                for (int j = 0; j < MAX_CLIENTS; j++) { //클라이언트를 찾음
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
                        if (websocket_handshake(cli) == 0) { //처음엔 핸드쉐이크를 수행
                            //핸드 셰이크 성공시++
                            cli->handshake_done = 1;

                            // 초기 캔버스 데이터 전송
                            pthread_mutex_lock(&canvas_mutex);
                            size_t init_len = WIDTH * HEIGHT * 3;
                            unsigned char *init_msg = malloc(init_len);
                            if (!init_msg) {
                                perror("Failed to allocate memory for init_msg");
                                pthread_mutex_unlock(&canvas_mutex);
                                close(cli->socket_fd);
                                cli->active = 0;
                                continue;
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
                            uint8_t header[10];
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
                                // 너무 큰 메시지는 처리하지 않음
                                free(init_msg);
                                close(cli->socket_fd);
                                cli->active = 0;
                                continue;
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
                                continue;
                            }

                            printf("Initial canvas sent to %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                            free(init_msg);
                        } 
                        else {
                            // 핸드셰이크 실패
                            printf("Handshake failed with %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                            close(cli->socket_fd);
                            cli->active = 0;
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
                                } else if (frame_len == 0) {
                                    // 더 이상 처리할 프레임이 없음
                                    break;
                                } else {
                                    // 오류 발생
                                    printf("Error processing frame for %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                                    close(cli->socket_fd);
                                    cli->active = 0;
                                    break;
                                }
                            }
                        }

                        if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                            // 오류 발생
                            perror("recv");
                            close(cli->socket_fd);
                            cli->active = 0;
                        } else if (bytes_read == 0) {
                            // 클라이언트 연결 종료
                            printf("Client disconnected: %s:%d\n", inet_ntoa(cli->address.sin_addr), ntohs(cli->address.sin_port));
                            close(cli->socket_fd);
                            cli->active = 0;
                            clientCnt--;
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
                }
            }
        }
    }
        // 정리
        close(server_fd);
        close(epoll_fd);
        pthread_mutex_destroy(&canvas_mutex);
        pthread_mutex_destroy(&clients_mutex);

        return 0;
}
