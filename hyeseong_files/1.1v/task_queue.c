#include "task_queue.h"
#include <stdlib.h>
#include <stdio.h>

// 작업 큐 초기화
void init_task_queue(TaskQueue *queue, int size) {

    queue->tasks = (Task *)malloc(sizeof(Task) * size);
    queue->size = size;

    queue->front = queue->rear = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

// 작업을 큐에 추가
void push_task(TaskQueue *queue, Task task) {

    const int size = queue->size;

    pthread_mutex_lock(&queue->lock);

    // 큐가 가득 찬 경우 처리
    if ((queue->rear + 1) % size == queue->front) {
        perror("Task queue is full! Dropping task.\n");
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    queue->tasks[queue->rear] = task;                 // 작업 추가
    queue->rear = (queue->rear + 1) % size;     // rear 포인터 이동

    pthread_cond_signal(&queue->cond);               // 대기 중인 스레드에 신호
    pthread_mutex_unlock(&queue->lock);
}

// 큐에서 작업을 가져오기
Task pop_task(TaskQueue *queue) {

    const int size = queue->size;

    pthread_mutex_lock(&queue->lock);

    // 큐가 비어 있는 경우 대기
    while (queue->front == queue->rear) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }

    Task task = queue->tasks[queue->front];           // 작업 가져오기
    queue->front = (queue->front + 1) % size;         // front 포인터 이동

    pthread_mutex_unlock(&queue->lock);
    return task;
}

// 작업 큐 파괴 함수
void destroy_task_queue(TaskQueue *queue) {

    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);

    free(queue->tasks);
    free(queue);

}