#ifndef CANVAS_H
#define CANVAS_H

#include "task_queue.h"
#include "include/uthash.h"
#include "client_manager.h"

// 픽셀 구조체
typedef struct {
    int x;
    int y;
    char color[8];
} Pixel;

// 수정된 픽셀 구조체
typedef struct {
    int key;            // x * CANVAS_HEIGHT + y
    char color[8];
    UT_hash_handle hh;  // uthash 핸들
} ModifiedPixel;

// 캔버스 구조체
typedef struct {
    Pixel *pixels;        // 캔버스 픽셀 데이터 배열
    ClientManager *cm;
    int canvas_width;
    int canvas_height;
    pthread_t tid;
    TaskQueue *queue;
    ModifiedPixel *modified_pixels; // 수정된 픽셀 해시 맵
} Canvas;


// 브로드캐스팅용 함수
uint8_t *create_websocket_frame(uint8_t *payload_data, size_t payload_len, size_t *frame_len);
void broadcast_updates(Canvas *canvas);

// 캔버스 초기화 함수
void init_canvas(Canvas *canvas, ClientManager *cm, int width, int height, int queue_size);

// 픽셀 업데이트 처리 함수(바이너리 데이터 처리)
//void process_pixel_update(Canvas *canvas, void *data);

#endif // CANVAS_H
