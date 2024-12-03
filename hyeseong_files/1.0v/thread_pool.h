#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "context.h"

// 스레드 풀 구조체
typedef struct {
    pthread_t *threads; // 스레드 배열
    int size;
} ThreadPool;

void *worker_thread(void *arg);

void init_thread_pool(ThreadPool *pool, Context *ctx, int size);

// 스레드 풀 삭제 함수
void destroy_thread_pool(ThreadPool *pool);

#endif // THREAD_POOL_H
