#ifndef CONTEXT_H
#define CONTEXT_H   

#define PORT_NUMBER 8080
#define EVENTS_SIZE 2048
#define TASK_QUEUE_SIZE 2048
#define CANVAS_WIDTH 500
#define CANVAS_HEIGHT 500

#include "client_manager.h"
#include "canvas.h"

typedef struct {
    ClientManager *cm; // 클라이언트 관리 구조체
    Canvas *canvas;   // 캔버스 구조체
} Context;

void init_context(Context *ctx);

#endif // CONTEXT_H
