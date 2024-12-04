#include "context.h"
#include <stdlib.h>
#include <stdio.h>

void init_context(Context *ctx) {
    ctx->cm = (ClientManager *)malloc(sizeof(ClientManager)); // ClientManager 동적 할당
    initClientManager(ctx->cm, PORT_NUMBER, EVENTS_SIZE, TASK_QUEUE_SIZE);

    ctx->canvas = (Canvas *)malloc(sizeof(Canvas)); // Canvas 동적 할당
    init_canvas(ctx->canvas, ctx->cm, CANVAS_WIDTH, CANVAS_HEIGHT, TASK_QUEUE_SIZE);

    printf("Context 초기화 완료\n");

}