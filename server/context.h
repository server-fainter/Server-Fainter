#ifndef CONTEXT_H
#define CONTEXT_H   

#include "server.h"
#include "client_manager.h"
#include "task_queue.h"
#include "canvas.h"

typedef struct {
    ServerManager *sm; // 서버 관리 구조체
    ClientManager *cm; // 클라이언트 관리 구조체
    TaskQueue *queue; // 작업 큐 구조체
    Canvas *canvas;   // 캔버스 구조체
} Context;

void init_context(Context *ctx);

#endif // CONTEXT_H
