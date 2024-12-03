#include "task_queue.h"
#include <stdlib.h>
#include <stdio.h>

// 작업 큐 초기화
void init_task_queue(TaskQueue *queue) {
    queue->front = queue->rear = 0;              
    pthread_mutex_init(&queue->lock, NULL);         
    pthread_cond_init(&queue->cond, NULL);          
}

// 작업을 큐에 추가
void push_task(TaskQueue *queue, Task task) {
    pthread_mutex_lock(&queue->lock);              

    queue->tasks[queue->rear] = task;              
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;   

    pthread_cond_signal(&queue->cond);              // 대기 중인 스레드에 작업 추가 신호 전달
    pthread_mutex_unlock(&queue->lock);             
}

// 큐에서 작업을 가져오기
Task pop_task(TaskQueue *queue) {
    pthread_mutex_lock(&queue->lock);               

    // 큐가 비어 있으면 작업이 추가될 때까지 대기
    while (queue->front == queue->rear) {           
        pthread_cond_wait(&queue->cond, &queue->lock); 
    }

    Task task = queue->tasks[queue->front];         
    queue->front = (queue->front + 1) % QUEUE_SIZE; 

    pthread_mutex_unlock(&queue->lock);            
    return task;                                   
}
