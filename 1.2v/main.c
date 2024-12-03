#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include "client_manager.h"
#include "canvas.h"
#include <stdbool.h>
#include "task_queue.h"
#include "http_handler.h"

static int count = 0;
// WebSocket 프레임을 처리하고 Task 구조체를 반환하는 함수
void process_websocket_frame(ClientManager *manager, int client_fd, char *buf, size_t buf_len) {
    uint8_t *buffer = (uint8_t *)buf;
    size_t buffer_len = buf_len;
    size_t payload_len = 0;
    size_t header_len = 2;

    // 기본 헤더가 도착했는지 확인 (헤더가 2바이트 미만이면 처리 불가)
    if (buffer_len < 2) {
        //printf("불완전한 헤더가 도착함\n");
        return;
    }

    bool fin = (buffer[0] & 0x80) != 0;         // FIN 플래그 확인 (1인경우 true)

    uint8_t opcode = buffer[0] & 0x0F;          // opcode는 하위 4비트
    uint8_t masked = (buffer[1] & 0x80) != 0;   // 마스킹 여부 (상위 비트 확인)
    payload_len = buffer[1] & 0x7F;             // 페이로드 길이는 하위 7비트

    // 확장된 페이로드 길이 처리
    if (payload_len == 126) { // 페이로드 길이가 126일 때
        header_len += 2;      // 확장 길이 2바이트 추가
        if (buffer_len < header_len) {
            return;
        }
        // 확장 길이를 big-endian으로 읽음
        payload_len = ((uint8_t)buffer[2] << 8) | (uint8_t)buffer[3];
    }
    else if (payload_len == 127) { // 페이로드 길이가 127일 때
        header_len += 8;             // 확장 길이 8바이트 추가
        if (buffer_len < header_len) {
            // printf("프레임 불완전: 확장된 페이로드 길이를 위한 %zu 바이트 필요\n", header_len);
            return;
        }

        // 64비트 확장 길이를 big-endian으로 읽음
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | (uint8_t)buffer[2 + i];
        }
    }

    // 마스킹 키 위치 계산
    size_t masking_key_offset = header_len; // 마스킹 키는 헤더 끝에 위치
    if (masked) {
        header_len += 4; // 마스킹 키가 4바이트이므로 헤더 길이에 추가
    }

    // 프레임 전체 길이 확인
    size_t total_frame_len = header_len + payload_len; // 헤더 길이 + 페이로드 길이
    if (buffer_len < total_frame_len) {
        //printf("프레임 불완전: 전체 프레임을 위한 %zu 바이트 필요\n", total_frame_len);
        return;
    }


    // 페이로드 데이터의 시작 위치 계산
    uint8_t *payload_data = buffer + header_len;

    // 마스킹이 적용된 경우 데이터 디마스킹 수행
    if (masked) {
        uint8_t *masking_key = buffer + masking_key_offset; // 마스킹 키 시작 위치
        //printf("마스킹 키를 사용하여 페이로드 데이터 디마스킹 수행\n");
        for (size_t i = 0; i < payload_len; i++) {
            payload_data[i] ^= masking_key[i % 4]; // 마스킹 키로 XOR 연산
        }
    }

    // 작업(Task) 구조체 생성
    Task task;
    task.client = client_fd;

    // opcode에 따라 작업 유형 설정 및 데이터 처리
    if (opcode == 0x8) {
        // 클라이언트 종료 프레임 처리 (opcode 0x8)
        task.type = TASK_WEBSOCKET_CLOSE;
        task.data = NULL; // 종료 프레임, 동일한 프레임 그대로 전송
        task.data_len = 0;
    }
    else if (opcode == 0x2) {

        // 바이너리 메시지 처리 (opcode 0x2)

        // 페이로드 데이터를 복사하여 새로운 버퍼에 할당
        uint8_t *payload_copy = (uint8_t *)malloc(payload_len);
        memcpy(payload_copy, payload_data, payload_len);

        if (fin) task.type = TASK_FRAME_MESSAGE;
        else task.type = TASK_MESSAGE_INCOMPLETE_FRAME;
        task.data = payload_copy;
        task.data_len = payload_len;

    }
    else if (opcode == 0x1) {
        // 텍스트 메시지 처리 (opcode 0x1)
        // 문자열을 NULL 종료하여 안전하게 처리
        char *text = (char *)malloc(payload_len + 1);
        memcpy(text, payload_data, payload_len);
        text[payload_len] = '\0'; // NULL 종료

        if (fin) task.type = TASK_FRAME_MESSAGE;
        else task.type = TASK_MESSAGE_INCOMPLETE_FRAME;
        task.data = text;
        task.data_len = payload_len;

    }
    else {
        // 알 수 없는 opcode 처리
        return;
    }
    push_task(manager->queue, task);
}

// WebSocket 프레임인지 확인하는 함수
bool is_websocket_frame(const uint8_t *data, size_t length) {
    if (length < 2) {
        return false; // WebSocket 프레임은 최소 2바이트 이상이어야 함
    }

    // FIN 비트와 Opcode 확인
    uint8_t first_byte = data[0];
    uint8_t opcode = first_byte & 0x0F; // Opcode는 하위 4비트

    if (opcode > 0xF) {
        return false; // Opcode는 0x0에서 0xF 사이여야 함
    }

    // Mask 비트 확인
    uint8_t second_byte = data[1];
    bool is_masked = (second_byte & 0x80) != 0;

    if (!is_masked) {
        return false; // 클라이언트에서 서버로의 WebSocket 프레임은 항상 Mask 비트를 가짐
    }

    // WebSocket 프레임으로 판별됨
    return true;
}

int main() {

     // Context 구조체 초기화
     Context *ctx = (Context *)malloc(sizeof(Context));
     init_context(ctx);

     // 이벤트 루프
     while (1) {
         int num_events = epoll_wait(ctx->cm->epoll_fd, ctx->cm->events, EVENTS_SIZE, -1);
         if (num_events == -1) {
             perror("epoll_wait failed");
             continue;
         }

         count += num_events;

         for (int i = 0; i < num_events; i++) {
             int fd = ctx->cm->events[i].data.fd;
             if (fd == ctx->cm->server_socket) { // 새로운 클라이언트 연결 처리
                 // 새로운 클라이언트 접속 처리
                 Task task = {0, TASK_NEW_CLIENT, NULL, 0};
                 push_task(ctx->cm->queue, task);
             }
             else {

                 char *buffer = malloc(sizeof(char) * REQUEST_BUFFER_SIZE);
                 memset(buffer, 0, REQUEST_BUFFER_SIZE * sizeof(char));
                 ssize_t len = recv(fd, buffer, REQUEST_BUFFER_SIZE, 0);

                 if (len <= 0) { 
                     // 클라이언트 접속 종료
                     Task task = {fd, TASK_CLIENT_CLOSE, "", len};
                     push_task(ctx->cm->queue, task);
                     free(buffer);
                 }
                 else {
                     // HTTP 요청 확인
                     if (is_http_request(buffer, len)) {

                         if (is_complete_http_request(buffer)) {

                             Task task = {fd, TASK_HTTP_REQUEST, buffer, len};
                             push_task(ctx->cm->queue, task);
                         }
                         else {
                             Task task = {fd, TASK_MESSAGE_INCOMPLETE_HTTP, buffer, len};
                             push_task(ctx->cm->queue, task);
                             // INCOMPLETE 보낸다음 클라이언트 상태 바꾼다음, 다음 버퍼(UNKNOWN_MESSAGE)를 기다린다
                         }
                     }
                     // websocket 요청 확인
                     else if (is_websocket_frame((uint8_t *)buffer, len)) {
                         process_websocket_frame(ctx->cm, fd, buffer, len);
                     }
                     else {
                         Task task = {fd, TASK_UNKNOWN_MESSAGE, buffer, len};
                         push_task(ctx->cm->queue, task);
                     }
                 }
              }
          }
          printf("Count: %d\n", count);
     }

     //리소스 정리
     destroyClientManger(ctx->cm);

    return 0;
}
