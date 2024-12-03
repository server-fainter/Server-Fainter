#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>

// 작업 큐 크기 정의
#define QUEUE_SIZE 1024

// 작업(Task) 구조체
typedef struct {
    int fd;   // 작업과 관련된 파일 디스크립터
    int type; // 작업 유형 (0: Accept, 1: Read 등)
} Task;

// 작업 큐(Task Queue) 구조체
typedef struct {
    Task tasks[QUEUE_SIZE];        // 작업을 저장하는 배열 (고정 크기 큐)
    int front, rear;               // 큐의 앞(front)과 뒤(rear)를 나타내는 인덱스
    pthread_mutex_t lock;          // 뮤텍스
    pthread_cond_t cond;           // 조건 변수 (스레드 대기 및 신호 전달)
} TaskQueue;

// 작업 큐 초기화 함수
// 큐의 상태와 동기화 메커니즘(뮤텍스와 조건 변수)을 초기화
void init_task_queue(TaskQueue *queue);

// 작업을 큐에 추가하는 함수
// 새로운 작업을 rear 위치에 추가하며, 대기 중인 스레드에 신호를 보냄
void push_task(TaskQueue *queue, Task task);

// 작업을 큐에서 가져오는 함수
// front 위치에서 작업을 가져오며, 큐가 비어 있으면 대기
Task pop_task(TaskQueue *queue);

#endif // TASK_QUEUE_H
