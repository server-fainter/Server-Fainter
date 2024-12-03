#ifndef CANVAS_H
#define CANVAS_H

#include "task_queue.h"
#include "include/uthash.h"
#include "client_manager.h"

// 팔레트 색상 인덱스 정의
typedef enum {
    RED,            // #ff0000
    ORANGE,         // #ffa600
    YELLOW,         // #ffe600
    LIGHT_GREEN,    // #8aff00
    GREEN,          // #24ff00
    DARK_GREEN,     // #00ff00
    CYAN,           // #00ff83
    LIGHT_CYAN,     // #00ffe6
    SKY_BLUE,       // #00e7ff
    BLUE,           // #008fff
    DARK_BLUE,      // #0024ff
    PURPLE,         // #2400ff
    MAGENTA,        // #8a00ff
    PINK,           // #ff00ff
    HOT_PINK,       // #ff0083
    MAROON,         // #ff0024
    DARK_RED,       // #d70000
    BROWN,          // #8c4000
    ORANGE_BROWN,   // #e67c00
    LIGHT_YELLOW,   // #b1d700
    DARK_YELLOW,    // #4cc100
    TURQUOISE,      // #00c16a
    AQUA,           // #00c1c1
    DARK_AQUA,      // #0055c1
    NAVY,           // #002fc1
    DARK_PURPLE,    // #7200c1
    PLUM,           // #c10072
    DARK_BROWN,     // #8c2600
    BLACK,          // #000000
    WHITE,          // #ffffff,
} Color;

// 픽셀 구조체
typedef struct {
    int x;
    int y;
    Color color;
} Pixel;

// 수정된 픽셀 구조체
typedef struct {
    int key;            // x * CANVAS_HEIGHT + y
    Color color;
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
