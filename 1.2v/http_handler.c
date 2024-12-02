#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "http_handler.h"
#include "http_parser.h"
#include "websocket_handshake.h"
#include <pthread.h>
#include "client_manager.h"

// MIME 타입 결정 함수
const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    // 필요에 따라 추가 MIME 타입을 정의

    return "application/octet-stream";
}

// HTTP 응답 전송 함수
void send_http_response(int client_fd, const char *status, const char *headers, const char *body, int body_length) {
    char response[1024 * 1024];
    int length = snprintf(response, sizeof(response),
                          "HTTP/1.1 %s\r\n"
                          "%s"
                          "\r\n",
                          status, headers);

    // 헤더 전송
    send(client_fd, response, length, 0);

    // 본문 전송
    if (body && body_length > 0) {
        send(client_fd, body, body_length, 0);
    }
}

// 파일 경로 보안 검증 함수
int is_valid_path(const char *path) {
    // 경로에 ".."이 포함되어 있는지 검사하여 디렉토리 트래버설 방지
    if (strstr(path, "..")) {
        return 0;
    }
    return 1;
}

// 정적 파일 요청 처리 함수
void handle_static_file_request(int client_fd, const char *path) {

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s%s", STATIC_FILES_DIR, path);

    // 파일 경로 검증
    if (!is_valid_path(file_path)) {
        const char *error_body = "<h1>403 Forbidden</h1>";
        send_http_response(client_fd, "403 Forbidden", "Content-Type: text/html\r\n", error_body, strlen(error_body));
        return;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        const char *error_body = "<h1>404 Not Found</h1>";
        send_http_response(client_fd, "404 Not Found", "Content-Type: text/html\r\n", error_body, strlen(error_body));
        return;
    }

    // 파일 크기 계산
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // 파일 내용 읽기
    char *file_content = malloc(file_size);
    fread(file_content, 1, file_size, file);
    fclose(file);

    // MIME 타입 결정
    const char *mime_type = get_mime_type(file_path);

    // 헤더 생성
    char headers[256];
    snprintf(headers, sizeof(headers), "Content-Type: %s\r\nContent-Length: %ld\r\n", mime_type, file_size);

    // 응답 전송
    send_http_response(client_fd, "200 OK", headers, file_content, file_size);

    free(file_content);
}

// WebSocket 업그레이드 요청 처리 함수
void handle_websocket_upgrade(Client *client, HttpRequest *http_request) {
    char accept_key[256];
    const char *client_key = NULL;

    // Sec-WebSocket-Key 헤더 검색
    for (int i = 0; i < http_request->header_count; i++) {
        if (strcasecmp(http_request->headers[i][0], "Sec-WebSocket-Key") == 0) {
            client_key = http_request->headers[i][1];
            break;
        }
    }

    if (!client_key) {
        // 키가 없으면 에러 응답
        const char *error_body = "<h1>400 Bad Request</h1>";
        send_http_response(client->socket_fd, "400 Bad Request", "Content-Type: text/html\r\n", error_body, strlen(error_body));
        return;
    }

    // Accept 키 생성
    generate_websocket_accept_key(client_key, accept_key);

    // 응답 헤더 생성
    char response[512];
    int length = snprintf(response, sizeof(response),
                          "HTTP/1.1 101 Switching Protocols\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Accept: %s\r\n"
                          "\r\n",
                          accept_key);

    printf("Websocket Connected :%d\n", client->socket_fd);

    // 응답 전송
    send(client->socket_fd, response, length, 0);

    // 클라이언트 상태 업데이트
    client->state = CONNECTION_OPEN;
}

// HTTP 요청 처리 함수
void handle_http_request(Client *client) {

    HttpRequest http_request;
    memset(&http_request, 0, sizeof(HttpRequest));

    // HTTP 요청 파싱
    http_parsing(client->recv_buffer, &http_request);

    // 메서드와 경로 출력 (디버그용)
    // printf("Received HTTP Request: %s %s\n", http_request.method, http_request.path);

    // GET 메서드인지 확인
    if (strcasecmp(http_request.method, "GET") != 0) {
        const char *error_body = "<h1>405 Method Not Allowed</h1>";
        send_http_response(client->socket_fd, "405 Method Not Allowed", "Content-Type: text/html\r\n", error_body, strlen(error_body));
        return;
    }

    // Upgrade 헤더 확인
    int is_upgrade = 0;
    for (int i = 0; i < http_request.header_count; i++) {
        // "Upgrade: websocket" 헤더가 있는 경우 weboscket 업그레이드 요청으로 판단.
        if (strcasecmp(http_request.headers[i][0], "Upgrade") == 0 &&
            strcasecmp(http_request.headers[i][1], "websocket") == 0) {
            is_upgrade = 1;
            break;
        }
    }

    if (is_upgrade) {
        // WebSocket 업그레이드 요청 처리
        handle_websocket_upgrade(client, &http_request);
    } else {
        // 정적 파일 요청 처리
        handle_static_file_request(client->socket_fd, http_request.path);
    }

    // 메모리 해제
    if (http_request.body) {
        free(http_request.body);
    }
}
