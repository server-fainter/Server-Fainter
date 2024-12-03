#ifndef CONTEXT_H
#define CONTEXT_H   

#include "client_manager.h"
#include "canvas.h"

#define PORT_NUMBER 8080
#define EVENTS_SIZE 2048
#define TASK_QUEUE_SIZE 2048
#define REQUEST_BUFFER_SIZE 40980
#define CANVAS_WIDTH 1000
#define CANVAS_HEIGHT 1000

typedef struct {
    ClientManager *cm; // 클라이언트 관리 구조체
    Canvas *canvas;   // 캔버스 구조체
} Context;

void init_context(Context *ctx);

#endif // CONTEXT_H
