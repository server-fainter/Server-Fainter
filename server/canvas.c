#include "canvas.h"
#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cjson/cJSON.h>
#include <stdbool.h>

// 캔버스 초기화
void canvas_init(Canvas *canvas) {

    if (canvas == NULL) {
        perror("Failed to allocate memory for canvas");
        exit(EXIT_FAILURE); // 메모리 할당 실패 시 프로그램 종료
    }

    // 뮤텍스 초기화
    pthread_mutex_init(&canvas->lock, NULL);

    // 모든 픽셀을 기본 색상으로 초기화
    for (int x = 0; x < CANVAS_WIDTH; x++) {
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            canvas->pixels[x][y].x = x;
            canvas->pixels[x][y].y = y;
            strcpy(canvas->pixels[x][y].color, "#FFFFFF"); // 기본 색상
        }
    }

    printf("Canvas Init\n");

}

// 유효한 좌표인지 확인
bool is_valid_coordinate(int x, int y) {
    return (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT);
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

// 픽셀 색상 업데이트
void canvas_update_pixel(Canvas *canvas, Pixel *p) {
    if (!is_valid_coordinate(p->x, p->y) || !is_valid_hex_color(p->color)) {
        // 유효하지 않은 입력은 무시
        return;
    }

    // printf("Update Pixel x=%d, y=%d, color=%s\n", p->x, p->y, p->color);

    pthread_mutex_lock(&canvas->lock); // 뮤텍스 잠금
    strcpy(canvas->pixels[p->x][p->y].color, p->color); // 색상 업데이트
    pthread_mutex_unlock(&canvas->lock); // 뮤텍스 잠금 해제

    free(p);
}

void canvas_destroy(Canvas *canvas) {
    // 뮤텍스 해제
    pthread_mutex_destroy(&canvas->lock);

    // 캔버스 메모리 해제
    free(canvas);
}

// 캔버스 상태 직렬화 함수
char* canvas_serialize_to_json(Canvas *canvas) {
    cJSON *root = cJSON_CreateObject();
    cJSON *pixels_array = cJSON_CreateArray();

    cJSON_AddItemToObject(root, "type", cJSON_CreateString("canvas_data"));
    cJSON_AddItemToObject(root, "pixels", pixels_array);

    pthread_mutex_lock(&canvas->lock); // 뮤텍스 잠금

    // 모든 픽셀 직렬화 (또는 흰색이 아닌 픽셀만)
    for (int x = 0; x < CANVAS_WIDTH; x++) {
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            Pixel *pixel = &canvas->pixels[x][y];
            cJSON *pixel_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(pixel_obj, "x", x);
            cJSON_AddNumberToObject(pixel_obj, "y", y);
            cJSON_AddStringToObject(pixel_obj, "color", pixel->color);
            cJSON_AddItemToArray(pixels_array, pixel_obj);
        }
    }

    pthread_mutex_unlock(&canvas->lock); // 뮤텍스 잠금 해제

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}