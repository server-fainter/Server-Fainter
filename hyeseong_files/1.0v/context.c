#include "context.h"

void init_context(Context *ctx) {
    ctx->sm = (ServerManager *)malloc(sizeof(ServerManager)); // ServerManager 동적 할당
    init_server(ctx->sm);

    ctx->cm = (ClientManager *)malloc(sizeof(ClientManager)); // ClientManager 동적 할당
    initClientManager(ctx->cm);

    ctx->queue = (TaskQueue *)malloc(sizeof(TaskQueue)); // TaskQueue 동적 할당
    init_task_queue(ctx->queue, 1024);

    ctx->canvas = (Canvas *)malloc(sizeof(Canvas)); // Canvas 동적 할당
    canvas_init(ctx->canvas);
}