#include "canvas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // usleep 함수 사용
#include <time.h>
#include <cjson/cJSON.h> // cJSON 헤더 파일 포함
#include <stdbool.h>

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

// JSON 데이터를 파싱하여 Pixel 구조체로 변환
Pixel *parse_pixel_json(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        fprintf(stderr, "Invalid JSON: %s\n", json_str);
        return NULL;
    }

    // "pixel" 객체 찾기
    cJSON *pixel_item = cJSON_GetObjectItem(json, "pixel");
    if (!cJSON_IsObject(pixel_item)) {
        fprintf(stderr, "Invalid pixel object in JSON: %s\n", json_str);
        cJSON_Delete(json);
        return NULL;
    }

    // "pixel" 객체 내의 "x", "y", "color" 찾기
    cJSON *x_item = cJSON_GetObjectItem(pixel_item, "x");
    cJSON *y_item = cJSON_GetObjectItem(pixel_item, "y");
    cJSON *color_item = cJSON_GetObjectItem(pixel_item, "color");

    if (!cJSON_IsNumber(x_item) || !cJSON_IsNumber(y_item) || !cJSON_IsString(color_item)) {
        fprintf(stderr, "Invalid pixel data in JSON: %s\n", json_str);
        cJSON_Delete(json);
        return NULL;
    }

    Pixel *parsed_pixel = (Pixel *)malloc(sizeof(Pixel));
    if (parsed_pixel == NULL) {
        fprintf(stderr, "Failed to allocate memory for Pixel.\n");
        cJSON_Delete(json);
        return NULL;
    }
    parsed_pixel->x = x_item->valueint;
    parsed_pixel->y = y_item->valueint;
    strcpy(parsed_pixel->color, color_item->valuestring);

    cJSON_Delete(json);
    return parsed_pixel;
}

void process_buffer(Canvas *canvas, const char *buffer, size_t length) {
    size_t start = 0;  // JSON 객체 시작 위치
    int brace_count = 0; // 중괄호 개수 추적

    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == '{') {
            // 열린 중괄호를 만나면 brace_count 증가
            if (brace_count == 0) {
                start = i; // JSON 시작 위치 기록
            }
            brace_count++;
        } else if (buffer[i] == '}') {
            // 닫힌 중괄호를 만나면 brace_count 감소
            brace_count--;

            // 완전한 JSON 객체를 감지
            if (brace_count == 0) {
                size_t json_obj_length = i - start + 1;

                // JSON 데이터 추출
                char *json_data = (char *)malloc(json_obj_length + 1);
                strncpy(json_data, buffer + start, json_obj_length);
                json_data[json_obj_length] = '\0';

                // Pixel 데이터 파싱
                Pixel *pixel = parse_pixel_json(json_data);
                free(json_data);

                if (pixel) {
                    if (is_valid_coordinate(pixel->x, pixel->y, canvas->canvas_width, canvas->canvas_height) &&
                        is_valid_hex_color(pixel->color)) {

                        // printf("Update Pixel: x=%d, y=%d, color=%s\n", pixel->x, pixel->y, pixel->color);

                        // 픽셀 업데이트 처리
                        int index = pixel->y * canvas->canvas_width + pixel->x;
                        strcpy(canvas->pixels[index].color, pixel->color);

                        // 수정된 픽셀 해시 맵에 추가
                        int key = index;
                        ModifiedPixel *p;
                        HASH_FIND_INT(canvas->modified_pixels, &key, p);
                        if (p == NULL) {
                            p = (ModifiedPixel *)malloc(sizeof(ModifiedPixel));
                            p->key = key;
                            strcpy(p->color, pixel->color);
                            HASH_ADD_INT(canvas->modified_pixels, key, p);
                        } else {
                            // 이미 존재하면 색상 업데이트
                            strcpy(p->color, pixel->color);
                        }

                    } else {
                        fprintf(stderr, "Invalid Pixel: x=%d, y=%d, color=%s\n", pixel->x, pixel->y, pixel->color);
                    }
                    free(pixel);
                }
            }
        }
    }
}

// 캔버스 매니저 스레드 함수 구현
static void *worker_thread(void *arg) {

    pthread_t tid = pthread_self();
    printf("Canvas Thread : %ld\n", tid);

    Canvas *canvas = (Canvas *)arg;

    // 일정 시간마다 브로드캐스트 업데이트 수행 (1초마다)
    static time_t last_broadcast = 0;

    while (1) {
        // 작업 큐에서 작업을 가져옴
        Task task = pop_task(canvas->queue);

        switch (task.type) {
            case TASK_PIXEL_UPDATE:
                // 픽셀 업데이트 작업 처리
                process_pixel_update(canvas, task.data);
                break;

            // 필요한 다른 작업 유형 처리 추가
            default:
                break;
        }

        time_t current_time = time(NULL);
        if (current_time - last_broadcast >= 1) {
            // printf("Broadcast Time\n");
            broadcast_updates(canvas);
            last_broadcast = current_time;
        }
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
        strcpy(canvas->pixels[i].color, "#FFFFFF"); // 기본 색상: 흰색
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

void process_pixel_update(Canvas *canvas, void *data) {
    // data는 클라이언트로부터 받은 버퍼(char *)라고 가정
    char *buffer = (char *)data;
    size_t length = strlen(buffer);

    process_buffer(canvas, buffer, length);

    // 버퍼 메모리 해제
    free(buffer);
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

    cJSON_AddItemToObject(json_message, "updated_pixel", json_pixels);

    // JSON 문자열로 변환
    char *message_str = cJSON_PrintUnformatted(json_message);

    Task task = {TASK_BROADCAST, message_str};
    push_task(canvas->cm->queue, task);

    // JSON 객체 메모리 해제
    cJSON_Delete(json_message);

    // 수정된 픽셀 목록 초기화
    HASH_ITER(hh, canvas->modified_pixels, p, tmp) {
        HASH_DEL(canvas->modified_pixels, p);
        free(p);
    }
}
