#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "task_queue.h"
#include "context.h"

// 스레드 풀 크기 정의
#define THREAD_POOL_SIZE 4

// 스레드 풀 구조체
typedef struct {
    pthread_t threads[THREAD_POOL_SIZE]; // 스레드 배열
} ThreadPool;

// 스레드 풀 초기화 함수
void init_thread_pool(ThreadPool *pool, Context *ctx);

// 스레드 풀 삭제 함수
void destroy_thread_pool(ThreadPool *pool);



#endif // THREAD_POOL_H
