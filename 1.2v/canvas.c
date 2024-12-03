#include "canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // usleep 함수 사용
#include <sys/time.h>
#include <stdint.h>
#include "client_manager.h"
#include "parsing_json.h"
#include "cjson/cJSON.h"
#include "save_canvas.h"

// 두 timeval 구조체 간의 시간 차이를 밀리초 단위로 반환
double time_diff_ms(struct timeval start, struct timeval end) {
    double start_ms = start.tv_sec * 1000.0 + start.tv_usec / 1000.0;
    double end_ms = end.tv_sec * 1000.0 + end.tv_usec / 1000.0;
    return end_ms - start_ms;
}

// 캔버스 매니저 스레드 함수 구현
static void *worker_thread(void *arg) {

    pthread_t tid = pthread_self();
    printf("Canvas Thread : %ld\n", tid);

    Canvas *canvas = (Canvas *)arg;

    // 일정 시간마다 브로드캐스트 업데이트 수행
    struct timeval last_broadcast, current_time;
    gettimeofday(&last_broadcast, NULL);

    //struct timeval last_save;
    //gettimeofday(&last_save, NULL);

    while (1) {
        // 작업 큐에서 작업을 가져옴
        Task task = pop_task(canvas->queue);

        switch (task.type) {
            case TASK_PIXEL_UPDATE: {
                process_json(canvas, (char *)task.data, task.data_len);
                free(task.data);
                break;
            }

            case TASK_NEW_CLIENT: {
                char *canvas_data = trans_canvas_as_json(canvas);
                size_t data_len = strlen(canvas_data);
                size_t frmae_len = 0;
                uint8_t * canvas_frame = create_websocket_frame(canvas_data, data_len, &frmae_len);
                free(canvas_data);
                Task t = {task.client, TASK_INIT_CANAVAS, canvas_frame, frmae_len};
                push_task(canvas->cm->queue, t);
            }

            // 필요한 다른 작업 유형 처리 추가
            default: {
                break;
            }

        }

        gettimeofday(&current_time, NULL);
        double time_gap = time_diff_ms(last_broadcast, current_time);
        if (time_gap >= 500.0) {                    // 100밀리초
            broadcast_updates(canvas);
            last_broadcast = current_time;
        }

        // gettimeofday(&current_time, NULL);
        // double time_gap_save = time_diff_ms(last_save, current_time);
        // if (time_gap_save >= 1000.0  * 5 * 1) {    // 1분 
        //     save_canvas_as_json(canvas);
        //     // 저장 로직 구현
        //     last_save = current_time;
        // }
    }

    pthread_exit(NULL);
}


// 캔버스 초기화 함수 구현
void init_canvas(Canvas *canvas, ClientManager *cm, int width, int height, int queue_size) {

    canvas->cm = cm;

    printf("캔버스 초기화 시작\n");

    canvas->canvas_width = width;
    canvas->canvas_height = height;
    canvas->pixels = malloc(sizeof(Pixel) * width * height);
    if (canvas->pixels == NULL) {
        fprintf(stderr, "캔버스 픽셀 메모리 할당 실패\n");
        exit(EXIT_FAILURE);
    }

    // 픽셀 초기화
    for (int i = 0; i < width * height; i++) {
        canvas->pixels[i].x = i % width;
        canvas->pixels[i].y = i / width;
        strcpy(canvas->pixels[i].color, "#FFFFFF");  // 컬러 흰색으로 초기화 
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

// WebSocket 프레임 생성 함수 구현
uint8_t *create_websocket_frame(uint8_t *payload_data, size_t payload_len, size_t *frame_len) {
    size_t header_size = 2; // 기본 헤더 크기

    // 페이로드 길이에 따라 헤더 크기 조정
    if (payload_len <= 125) {
        // 아무 것도 하지 않음, header_size는 이미 2로 설정됨
    } else if (payload_len <= 65535) {
        // 126 표시 + 16비트의 확장된 페이로드 길이
        header_size += 2;
    } else {
        // 127 표시 + 64비트의 확장된 페이로드 길이
        header_size += 8;
    }

    // 프레임 전체 길이 계산
    *frame_len = header_size + payload_len;

    // 프레임 버퍼 할당
    uint8_t *frame = (uint8_t *)malloc(*frame_len);
    if (frame == NULL) {
        fprintf(stderr, "WebSocket 프레임 메모리 할당 실패\n");
        return NULL;
    }

    // 첫 번째 바이트 설정: FIN(1) + RSV1-3(0) + Opcode(0x1: 텍스트 프레임)
    frame[0] = 0x81; // FIN = 1, Opcode = 0x1 (텍스트 프레임)

    // 두 번째 바이트 및 확장된 페이로드 길이 설정
    if (payload_len <= 125) {
        frame[1] = (uint8_t)payload_len;
    } else if (payload_len <= 65535) {
        frame[1] = 126;
        frame[2] = (payload_len >> 8) & 0xFF; // 상위 바이트
        frame[3] = payload_len & 0xFF;        // 하위 바이트
    } else {
        frame[1] = 127;
        // 64비트 길이를 빅엔디언으로 저장
        for (int i = 0; i < 8; i++) {
            frame[2 + i] = (payload_len >> (56 - i * 8)) & 0xFF;
        }
    }

    // 페이로드 데이터 복사
    memcpy(frame + header_size, payload_data, payload_len);

    return frame;
}

// 수정된 픽셀을 브로드캐스트하는 함수 구현
void broadcast_updates(Canvas *canvas) {
    // 수정된 픽셀이 없으면 함수 종료
    if (canvas->modified_pixels == NULL) {
        return;
    }

    // JSON 객체 생성
    cJSON *json_message = cJSON_CreateObject();
    cJSON *json_pixels = cJSON_CreateArray();

    ModifiedPixel *p, *tmp;
    HASH_ITER(hh, canvas->modified_pixels, p, tmp) {
        // 해시 맵 키를 이용하여 x와 y 좌표 계산
        int y = p->key / canvas->canvas_width;
        int x = p->key % canvas->canvas_width;

        cJSON *json_pixel = cJSON_CreateObject();
        cJSON_AddNumberToObject(json_pixel, "x", x);
        cJSON_AddNumberToObject(json_pixel, "y", y);
        cJSON_AddStringToObject(json_pixel, "color", p->color);

        cJSON_AddItemToArray(json_pixels, json_pixel);
    }

    // 클라이언트 수 추가
    cJSON_AddNumberToObject(json_message, "client_count", get_client_count(canvas->cm));
    cJSON_AddItemToObject(json_message, "updated_pixel", json_pixels);

    // JSON 문자열로 변환
    char *message_str = cJSON_PrintUnformatted(json_message);
    size_t message_len = strlen(message_str);

    // Frame 생성
    size_t frame_len = 0;
    uint8_t *data = create_websocket_frame((uint8_t*)message_str, message_len, &frame_len);

    // 클라이언트 매니저에게 넘겨준다.
    Task task = {0, TASK_BROADCAST, data, frame_len};
    push_task(canvas->cm->queue, task);

    // JSON 객체 메모리 해제
    cJSON_Delete(json_message);

    // 수정된 픽셀 목록 초기화
    HASH_ITER(hh, canvas->modified_pixels, p, tmp) {
        HASH_DEL(canvas->modified_pixels, p);
        free(p);
    }
}


// // 픽셀 업데이트 처리 함수 구현
// void process_pixel_update(Canvas *canvas, void *data) {
//     uint8_t *byte_data = (uint8_t *)data;
//
//     // 데이터에서 x, y, color 추출
//     uint16_t x = (byte_data[1] << 8) | byte_data[0]; // Little-endian: LSB 먼저
//     uint16_t y = (byte_data[3] << 8) | byte_data[2];
//     uint8_t color = byte_data[4];
//
//     printf("Pixel Update:\n");
//     printf("  X: %u\n", x);
//     printf("  Y: %u\n", y);
//     printf("  Color: %u\n", color);
//
//     // 좌표 유효성 검사
//     if (!is_valid_coordinate(x, y, canvas->canvas_width, canvas->canvas_height)) {
//         fprintf(stderr, "유효하지 않은 좌표입니다: x=%u, y=%u\n", x, y);
//         free(data);
//         return;
//     }
//
//     // 캔버스의 픽셀 업데이트
//     int index = y * canvas->canvas_width + x;
//     canvas->pixels[index].color = color;
//
//     // WebSocket 프레임 생성
//     size_t payload_len = 1 + 5; // 타입(1바이트) + x(2바이트) + y(2바이트) + color(1바이트)
//     uint8_t payload_data[payload_len];
//
//     // 타입 그대로 복사
//     payload_data[0] = 1; // 타입 (1바이트)
//
//     // 나머지 5바이트 복사
//     memcpy(&payload_data[1], &byte_data[1], 5); // 뒤의 x, y, color 데이터 복사
//
//     // WebSocket 프레임 생성
//     size_t frame_size;
//     uint8_t *frame = create_websocket_frame(payload_data, payload_len, &frame_size);
//     if (frame == NULL) {
//         fprintf(stderr, "WebSocket 프레임 생성 실패\n");
//         free(data);
//         return;
//     }
//
//     // 디버깅: 프레임 내용 확인
//     // printf("Generated WebSocket Frame (size: %zu bytes):\n", frame_size);
//     // for (size_t i = 0; i < frame_size; i++) {
//     //     printf("  Byte %zu: 0x%02X\n", i, frame[i]);
//     // }
//
//     // 클라이언트 매니저의 작업 큐에 전송 작업 추가
//     Task send_task;
//     send_task.client = 0;
//     send_task.type = TASK_BROADCAST; // TASK_BROADCAST 타입 정의 필요
//     send_task.data = frame;
//     send_task.data_len = frame_size; // Task 구조체에 data_len 필드 추가 필요
//     push_task(canvas->cm->queue, send_task);
// }

