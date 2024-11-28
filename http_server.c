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
    int socket_fd;//소켓 id
    struct sockaddr_in address;//소켓 주소
    pthread_t thread;//쓰레드 id
    int active;//활성여부
} client_t;

client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex;
// Base64 인코딩 함수 구현
void base64_encode(const unsigned char *input, int length, char *output, int output_size) {
    int encoded_length = EVP_EncodeBlock((unsigned char *)output, input, length);
    if (encoded_length < 0 || encoded_length >= output_size) {
        fprintf(stderr, "Base64 encoding failed\n");
    }
    output[encoded_length] = '\0';
}
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
//HTTP로 클라이언트에게 수신 후 업그레이드해서 발신
int websocket_handshake(int client_fd) {
    /*
    GET /chat HTTP/1.1//우린 웹소켓이라 get post는 안씀
    Host: server.com 
    Upgrade: websocket 
    Connection: Upgrade 
    Sec-WebSocket-Key: dGgnnsduwqka17gdsSB3j==  
    Origin: http://example.com  
    Sec-WebSocket-Protocol: chat, superchat 
    Sec-WebSocket-Version: 13 
    */
    //
    char buffer[BUFFER_SIZE];
    //
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return -1;
    }
    //끝문자 NULL  
    buffer[received] = '\0';
    
    // if (strstr(buffer, "Upgrade: websocket") == NULL) {
    //     return -1;
    // }

    char *key_header = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_header) {
        return -1;
    }
    //seckey다음 위치로 이동
    key_header += strlen("Sec-WebSocket-Key: ");

    //seckey를 찾아서
    char *key_end = strstr(key_header, "\r\n");
    if (!key_end) {
        return -1;
    }

    char client_key[256];
    size_t key_len = key_end - key_header;
    if (key_len >= sizeof(client_key)) {
        key_len = sizeof(client_key) - 1;
    }
    //키 값 client_key에 넣기
    strncpy(client_key, key_header, key_len);
    client_key[key_len] = '\0';

    const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char key_guid[300];
    int n = snprintf(key_guid, sizeof(key_guid), "%s%s", client_key, guid);
    if (n < 0 || n >= sizeof(key_guid)) {
        printf("------*** 89 ***-----\n");
        return -1;
    }
    //sha1로 해싱
    unsigned char sha1_result[SHA_DIGEST_LENGTH];
    sha1_hash(key_guid, strlen(key_guid), sha1_result);
    printf("------*** 95 : sha1 해싱완료 ***-----\n");
    //base64로 인코딩
    char accept_key[256];
    base64_encode(sha1_result, SHA_DIGEST_LENGTH, accept_key, sizeof(accept_key));
    printf("------*** 99 :base64로 인코딩 완료 ***-----\n");
    
    char response[512];
    n = snprintf(response, sizeof(response),
                 "HTTP/1.1 101 Switching Protocols\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 "Sec-WebSocket-Accept: %s\r\n\r\n",
                 accept_key);
    if (n < 0 || n >= sizeof(response)) {
        return -1;
    }

    if (send(client_fd, response, strlen(response), 0) < 0) {
        return -1;
    }

    return 0;
}

// HTTP 서버 설정 및 클라이언트 요청 처리
int setup_http_server(int port) {
    int server_fd;
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }

    int opt = 1;//옵션 키기
    //setsockopt는 소켓의 파일 디스크립터
    // (fd, 옵셜레벨, 설정할옵션이름,버퍼,버퍼크기)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(server_fd);
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    //서버 열기
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }
    //서버 리슨
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

// HTTP 요청 처리 함수
void handle_http_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        return;
    }
    //
    buffer[received] = '\0';
    printf("Received HTTP request:\n%s\n", buffer);

    // HTTP 200 OK 응답으로 HTML 파일 제공
    const char *html_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html><body>"
        "<h1>Welcome to the Canvas WebSocket Server</h1>"
        "<p>To interact with the canvas, establish a WebSocket connection.</p>"
        "</body></html>";

    send(client_fd, html_response, strlen(html_response), 0);
}

// 서버에서 클라이언트 처리
void *handle_client(void *arg) {//클라이언트 소켓 정보가 넘어옴
    client_t *cli = (client_t *)arg;//클라이언트 소켓정보 client_t
    uint8_t buffer[BUFFER_SIZE];
    
    // HTTP 요청 처리
    // 클라이언트의 html요청을 받고
    // html내용을 담아 전송
    handle_http_request(cli->socket_fd);

    // WebSocket 핸드셰이크 처리
    if (websocket_handshake(cli->socket_fd) < 0) {
        printf("WebSocket Handshake failed\n");
        close(cli->socket_fd);
        return NULL;
    }

    // 클라이언트 연결 이후의 WebSocket 처리는 이전 코드처럼 수행
    // ...

    return NULL;
}

int main(void) {
    //서버 소켓 열기
    int http_server_fd = setup_http_server(PORT);
    if (http_server_fd < 0) {
        fprintf(stderr, "Failed to set up HTTP server\n");
        return -1;
    }

    while (1) {
        //클라이언트 소켓 설정
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        //클라이언트 소켓 디스크립터
        int new_socket;
        //1. 클라이언트 connect 받기
        //한 while당 한 socket만 받음!!
        if ((new_socket = accept(http_server_fd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        //2. 클라이언트 액티브 탐색 
        //client 배열을 지키기 위한 뮤텍스 락
        pthread_mutex_lock(&clients_mutex);
        int idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].active) {
                idx = i;
                break;
            }
        }
        //클라이언트중 액티브 상태라면//왜 액티브->할당??
        if (idx != -1) {
            //3. clients배열에 새로 들어온 소켓연결 할당
            clients[idx].socket_fd = new_socket;
            clients[idx].address = client_addr;
            clients[idx].active = 1;
            //4. 클라이언트당 쓰레드할당
            if (pthread_create(&clients[idx].thread, NULL, handle_client, &clients[idx]) != 0) {
                perror("Thread creation failed");
                //연결 실패시
                clients[idx].active = 0;
                //실패한 소켓은 클로즈 
                close(new_socket);
            } else {
                //5. 쓰레드생성이 정상이면 디태치로 분리 
                pthread_detach(clients[idx].thread); 
            }
        } else {
            printf("Maximum clients connected. Connection refused.\n");
            //최대수용가능 클라이언트 커넥션이 넘김. close
            close(new_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    close(http_server_fd);
    return 0;
}
