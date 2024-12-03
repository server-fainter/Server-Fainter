#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "task_queue.h"
#include "context.h"

// 스레드 풀의 크기 정의
#define THREAD_POOL_SIZE 4

// 스레드 풀 구조체
typedef struct {
    pthread_t threads[THREAD_POOL_SIZE]; // 스레드 배열
} ThreadPool;

// 스레드 풀 초기화 함수
// 스레드를 생성하고 작업 큐와 컨텍스트를 연결
void init_thread_pool(ThreadPool *pool, Context *queue);

// 스레드 풀 삭제 함수
// 스레드를 종료하고 모든 리소스를 해제
void destroy_thread_pool(ThreadPool *pool);

#endif // THREAD_POOL_H
