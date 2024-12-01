#include "canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // usleep 함수 사용
#include <time.h>
#include <cjson/cJSON.h> // cJSON 헤더 파일 포함
#include <stdbool.h>
#include <stdint.h>

// 유효한 좌표인지 확인
bool is_valid_coordinate(int x, int y, int width, int height) {
    return (x >= 0 && x < width && y >= 0 && y < height);
}

// 유효한 HEX 색상인지 확인
bool is_valid_hex_color(const char *color) {
    if (color[0] != '#' || strlen(color) != 7) {
        return false;
    }
    for (int i = 1; i < 7; i++) {
        if (!((color[i] >= '0' && color[i] <= '9') ||
              (color[i] >= 'A' && color[i] <= 'F') ||
              (color[i] >= 'a' && color[i] <= 'f'))) {
            return false;
              }
    }
    return true;
}

// WebSocket 프레임을 처리하는 함수
// 클라이언트가 보낸 WebSocket 프레임을 분석하고 opcode에 따라 처리합니다.
ssize_t process_websocket_frame(Canvas *canvas, Client *client) {
    uint8_t *buffer = (uint8_t *)client->recv_buffer;
    size_t buffer_len = client->recv_buffer_len; 
    size_t payload_len = 0; 
    size_t header_len = 2;  

    // 기본 헤더가 도착했는지 확인 (헤더가 2바이트 미만이면 처리 불가)
    if (buffer_len < 2) {
        return 0; 
    }

    // FIN 플래그 확인 (첫 번째 바이트의 상위 비트)
    uint8_t fin = (buffer[0] & 0x80) != 0; // FIN 플래그 추출 (1이면 종료 프레임)
    if (!fin) {
        return 0; // FIN 플래그가 0이면 처리하지 않고 0 반환
    }

    uint8_t opcode = buffer[0] & 0x0F;          // opcode는 하위 4비트
    uint8_t masked = (buffer[1] & 0x80) != 0;   // 마스킹 여부(상위 비트 확인)
    payload_len = buffer[1] & 0x7F;             // 페이로드 길이는 하위 7비트

    // 확장된 페이로드 길이 처리
    if (payload_len == 126) { // 페이로드 길이가 126일 때
        header_len += 2;      // 확장 길이 2바이트 추가
        if (buffer_len < header_len) return 0; // 전체 헤더가 아직 도착하지 않음

        // 확장 길이를 big-endian으로 읽음
        payload_len = ((uint8_t)buffer[2] << 8) | (uint8_t)buffer[3];
    } else if (payload_len == 127) { // 페이로드 길이가 127일 때
        header_len += 8;             // 확장 길이 8바이트 추가
        if (buffer_len < header_len) return 0; // 전체 헤더가 아직 도착하지 않음

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
        return 0; // 전체 프레임이 도착하지 않았으면 처리를 중단
    }

    // 페이로드 데이터의 시작 위치 계산
    uint8_t *payload_data = (uint8_t *)(buffer + header_len);

    // 마스킹이 적용된 경우 데이터 디마스킹 수행
    if (masked) {
        uint8_t *masking_key = buffer + masking_key_offset; // 마스킹 키 시작 위치
        for (size_t i = 0; i < payload_len; i++) {
            payload_data[i] ^= masking_key[i % 4];          // 마스킹 키로 XOR 연산
        }
    }

    // opcode에 따라 메시지 처리
    if (opcode == 0x8) { 
        // 클라이언트 종료 프레임 처리 (opcode 0x8)
        Task task = {client, TASK_CLIENT_CLOSE, ""}; // 클라이언트 종료 작업 생성
        push_task(canvas->cm->queue, task);
        return total_frame_len;                      // 처리한 프레임 길이 반환
    } 
    else if (opcode == 0x2) { 
        // 바이너리 메시지 처리 (opcode 0x2)
        size_t i = 0;

        while (i + 2 < payload_len) { 
            // 페이로드 데이터를 3바이트씩 읽어서 (x, y, color) 정보로 해석
            uint8_t x = payload_data[i];
            uint8_t y = payload_data[i + 1];
            uint8_t color = payload_data[i + 2];
            i += 3;

            printf("x = %d, y = %d, color = %d\n", x, y, color);

            // 유효한 좌표인지 확인 후 캔버스를 업데이트
        }
    }

    // 처리한 프레임 전체 길이 반환
    return total_frame_len;
}


// 캔버스 매니저 스레드 함수 구현
static void *worker_thread(void *arg) {

    pthread_t tid = pthread_self();
    printf("Canvas Thread : %ld\n", tid);

    Canvas *canvas = (Canvas *)arg;

    // 일정 시간마다 브로드캐스트 업데이트 수행 (1초마다)
    // static time_t last_broadcast = 0;

    while (1) {
        // 작업 큐에서 작업을 가져옴
        Task task = pop_task(canvas->queue);
        Client *client = task.client;

        switch (task.type) {
            case TASK_PIXEL_UPDATE:

                pthread_mutex_lock(&canvas->cm->lock);
                ssize_t n = process_websocket_frame(canvas, client);
                
                if(n > 0) {
                    // 처리된 프레임을 버퍼에서 제거
                    memmove(client->recv_buffer, client->recv_buffer + n, client->recv_buffer_len - n);
                    client->recv_buffer_len -= n;
                } else {
                    // 프레임이 불완전하거나, 오류가 있음
                }

                pthread_mutex_unlock(&canvas->cm->lock);
                break;

            // 필요한 다른 작업 유형 처리 추가
            default:
                break;
        }

        // time_t current_time = time(NULL);
        // if (current_time - last_broadcast >= 1) {
        //     // printf("Broadcast Time\n");
        //     broadcast_updates(canvas);
        //     last_broadcast = current_time;
        // }
    }
    // 스레드 종료 시 자원 정리 (필요한 경우)
    pthread_exit(NULL);
    return NULL;
}


// 캔버스 초기화 함수 구현
void init_canvas(Canvas *canvas, ClientManager *cm, int width, int height, int queue_size) {

    canvas->cm = cm;

    printf("캔버스 초기화 시작\n");

    canvas->canvas_width = width;
    canvas->canvas_height = height;
    canvas->pixels = (Pixel *)malloc(sizeof(Pixel) * width * height);
    if (canvas->pixels == NULL) {
        fprintf(stderr, "캔버스 픽셀 메모리 할당 실패\n");
        exit(EXIT_FAILURE);
    }

    // 픽셀 초기화
    for (int i = 0; i < width * height; i++) {
        canvas->pixels[i].x = i % width;
        canvas->pixels[i].y = i / width;
        canvas->pixels[i].color = 29; // 기본 색상: 흰색
    }

    printf("캔버스 배열 할당 및 초기화 성공\n");

    // 작업 큐 초기화
    canvas->queue = (TaskQueue *)malloc(sizeof(TaskQueue));
    init_task_queue(canvas->queue, queue_size);

    printf("캔버스 Task Queue 초기화\n");

    canvas->modified_pixels = NULL; // 수정된 픽셀 해시 맵 초기화

    // 캔버스 매니저 스레드 생성
    const int n = pthread_create(&canvas->tid, NULL, worker_thread, (void *)canvas);
    if (n != 0) {
        fprintf(stderr, "캔버스 스레드 생성 실패: %s\n", strerror(n));
        free(canvas->pixels);
        exit(EXIT_FAILURE);
    }

    printf("캔버스 스레드 생성 성공\n");
}

// 수정된 픽셀을 브로드캐스트하는 함수 구현
// void broadcast_updates(Canvas *canvas) {
//     // 수정된 픽셀이 없으면 함수 종료
//     if (canvas->modified_pixels == NULL) {
//         return;
//     }

//     // JSON 객체 생성
//     cJSON *json_message = cJSON_CreateObject();
//     cJSON *json_pixels = cJSON_CreateArray();

//     ModifiedPixel *p, *tmp;
//     HASH_ITER(hh, canvas->modified_pixels, p, tmp) {
//         // 해시 맵 키를 이용하여 x와 y 좌표 계산
//         int y = p->key / canvas->canvas_width;
//         int x = p->key % canvas->canvas_width;

//         cJSON *json_pixel = cJSON_CreateObject();
//         cJSON_AddNumberToObject(json_pixel, "x", x);
//         cJSON_AddNumberToObject(json_pixel, "y", y);
//         cJSON_AddStringToObject(json_pixel, "color", p->color);

//         cJSON_AddItemToArray(json_pixels, json_pixel);
//     }

//     cJSON_AddItemToObject(json_message, "updated_pixel", json_pixels);

//     // JSON 문자열로 변환
//     char *message_str = cJSON_PrintUnformatted(json_message);

//     Task task = {NULL, TASK_BROADCAST, message_str};
//     push_task(canvas->cm->queue, task);

//     // JSON 객체 메모리 해제
//     cJSON_Delete(json_message);

//     // 수정된 픽셀 목록 초기화
//     HASH_ITER(hh, canvas->modified_pixels, p, tmp) {
//         HASH_DEL(canvas->modified_pixels, p);
//         free(p);
//     }
// }
