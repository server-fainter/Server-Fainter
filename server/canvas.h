#ifndef CANVAS_H
#define CANVAS_H

#include <pthread.h>
#include <stdbool.h>

#define CANVAS_WIDTH 100
#define CANVAS_HEIGHT 100

// 픽셀 구조체
typedef struct {
    int x;
    int y;
    char color[8];
}Pixel;

typedef struct {
    Pixel pixels[CANVAS_WIDTH][CANVAS_HEIGHT]; // 캔버스 픽셀 데이터
    pthread_mutex_t lock; // 캔버스 동시성 제어를 위한 뮤텍스
} Canvas;

// 캔버스 초기화 함수
void canvas_init(Canvas **canvas);

// 유효한 색상, 좌표인지 확인하는 함수
bool is_valid_coordinate(int x, int y);
bool is_valid_hex_color(const char *color);

// 캔버스의 특정 픽셀 업데이트
void canvas_update_pixel(Canvas *canvas, Pixel *p);

void canvas_destroy(Canvas *canvas);

// 캔버스 상태 직렬화 함수
char* canvas_serialize_to_json(Canvas *canvas);

#endif // CANVAS_H
