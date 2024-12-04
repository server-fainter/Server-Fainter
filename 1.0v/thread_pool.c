#include "thread_pool.h"
#include "task_queue.h"
#include "context.h"
#include "canvas.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cjson/cJSON.h>

// JSON 데이터를 파싱하여 Pixel 구조체로 변환
Pixel *parse_pixel_json(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        fprintf(stderr, "Invalid JSON: %s\n", json_str);
        return NULL;
    }

    cJSON *pixel = cJSON_GetObjectItem(json, "pixel");
    if (!cJSON_IsObject(pixel)) {
        fprintf(stderr, "Missing 'pixel' object in JSON: %s\n", json_str);
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *x = cJSON_GetObjectItem(pixel, "x");
    cJSON *y = cJSON_GetObjectItem(pixel, "y");
    cJSON *color = cJSON_GetObjectItem(pixel, "color");

    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsString(color)) {
        fprintf(stderr, "Invalid pixel data in JSON: %s\n", json_str);
        cJSON_Delete(json);
        return NULL;
    }

    Pixel *parsed_pixel = (Pixel *)malloc(sizeof(Pixel));
    parsed_pixel->x = x->valueint;
    parsed_pixel->y = y->valueint;
    strcpy(parsed_pixel->color, color->valuestring); // 문자열 복사

    cJSON_Delete(json);
    return parsed_pixel;
}

// 중괄호를 기준으로 JSON 추출
void process_buffer(Canvas *canvas, const char *buffer, size_t length) {
    size_t start = 0;  // JSON 객체 시작 위치
    int brace_count = 0; // 중괄호 개수 

    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == '{') {
            // 열린 중괄호를 만나면 brace_count 증가
            if (brace_count == 0) {
                // JSON 시작 위치 기록
                start = i; 
            }
            brace_count++;
        } else if (buffer[i] == '}') {
            // 닫힌 중괄호를 만나면 brace_count 감소
            brace_count--;

            // 완전한 JSON 객체를 감지 (닫는 중괄호 2번 만난 경우)
            if (brace_count == 0) {
                size_t json_obj_length = i - start + 1;

                // JSON 데이터 추출
                char json_data[json_obj_length + 1];
                strncpy(json_data, buffer + start, json_obj_length);
                json_data[json_obj_length] = '\0';

                // Pixel 데이터 파싱
                Pixel *pixel = parse_pixel_json(json_data);
                if (pixel) {
                    if (is_valid_coordinate(pixel->x, pixel->y) && is_valid_hex_color(pixel->color)) {
                        canvas_update_pixel(canvas, pixel);
                    } else {
                        fprintf(stderr, "Invalid Pixel: x=%d, y=%d, color=%s\n", pixel->x, pixel->y, pixel->color);
                    }
                }
            }
        }
    }
}

// 워커 스레드: 작업 큐에서 작업을 가져와 처리
void *worker_thread(void *arg) {

    Context *ctx = (Context *)arg;
    TaskQueue *queue = ctx->queue;

    while (1) {
        Task task = pop_task(queue);

        switch (task.type) {
            case TASK_NEW_CLIENT: {
                int client_socket = accept_new_client(ctx->sm);
                addClient(ctx->cm, client_socket);
                break;
            }
            case TASK_TEST_RECV_MESSAGE: {
                //printf("Received data: %s\n", task.data);
                break;
            }
            case TASK_PIXEL_UPDATE: {
                char *buffer = (char *)task.data;
                broadcastClients(ctx->cm, buffer);

                // printf("Thread : %ld -> %s\n", i, buffer);
                process_buffer(ctx->canvas, buffer, strlen(buffer));

                free(buffer);

                break;
            }
            
            case TASK_SEND_STATIC_FILE: {
                /*
                1. 클라이언트가 새로 접속했을 경우
                2. 요청 받은 HTML, CSS, JS, 전체 캔버스 정보가 담긴 JSON 파일 SEND
                */
                break;
            }

            case TASK_BROADCAST: {
                /*

                1. 수정된 픽셀에 대한 데이터(캐시 버퍼)를 JSON 형식의 문자열로 변환(직렬화)
                2. 현재 접속중인 모든 클라이언트에게 브로드 캐스팅
                (아니면 픽셀이 수정될 때마다 브로드 캐스팅 하는 것은 어떤지 고려가 필요하다)

                */
                break;
            }
            default: {
                fprintf(stderr, "Unknown task type: %d\n", task.type);
                break;
            }
        }
    }

    return NULL;
}

void init_thread_pool(ThreadPool *pool, Context *ctx, const int size) {

    pool->size = size;
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * size);

    // 스레드 생성
    for (int i = 0; i < size; i++) {
        if(pthread_create(&pool->threads[i], NULL, worker_thread, ctx) == -1){
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }  
    }

    printf("Thread Init\n");
}

// 스레드 풀 삭제
void destroy_thread_pool(ThreadPool *pool) {

    const int size = pool->size;

    for (int i = 0; i < size; i++) {
        pthread_cancel(pool->threads[i]);
        pthread_join(pool->threads[i], NULL);
    }
}
