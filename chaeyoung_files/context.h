#ifndef CONTEXT_H
#define CONTEXT_H   

#include "server.h"
#include "clientmanager.h"
#include "task_queue.h"

typedef struct {
    ServerManager sm; // 서버 관리 구조체
    ClientManager cm; // 클라이언트 관리 구조체
    TaskQueue queue; // 작업 큐 구조체
} Context;

#endif // MAIN_H
