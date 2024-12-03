#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>
#include <stdio.h>

typedef enum {
    TASK_NEW_CLIENT,                // 새로운 클라이언트가 접속 요청하는 경우
    TASK_PIXEL_UPDATE,              // 픽셀 업데이트 작업
    TASK_HTTP_REQUEST,              // HTTP 요청
    TASK_BROADCAST,                 // 브로드캐스팅(수정된 픽셀 정보)
    TASK_CLIENT_CLOSE,              // 클라이언트 접속 종료
    TASK_WEBSOCKET_CLOSE,
    TASK_MESSAGE_INCOMPLETE_HTTP,        // 메세지가 불완전함(조각남, http)
    TASK_MESSAGE_INCOMPLETE_FRAME,       // 메세지가 불완전함(조각남, frame)
    TASK_FRAME_MESSAGE,             // frame 메세지 요청(완전함, 혹은 마지막 조각임)
    TASK_UNKNOWN_MESSAGE,           // 알수없는 형식(http 조각 일 수 있음)
    TASK_INIT_CANAVAS
}TaskType;

// 작업(Task) 구조체
typedef struct {
    int client;        // 클라이언트 소켓
    TaskType type;     // 작업 유형
    void *data;        // 클라이언트로부터 받은 데이터 (예: JSON)
    ssize_t data_len;  // 데이터 길이
} Task;

// 작업 큐(Task Queue) 구조체
typedef struct {
    Task *tasks; // 작업 배열 (고정 크기 큐)
    int front, rear;        // 큐의 앞(front)과 뒤(rear) 인덱스
    pthread_mutex_t lock;   // 뮤텍스
    pthread_cond_t cond;    // 조건 변수
    int size;
} TaskQueue;

// 작업 큐 초기화 함수
void init_task_queue(TaskQueue *queue, int size);

// 작업을 큐에 추가하는 함수
void push_task(TaskQueue *queue, Task task);

// 작업을 큐에서 가져오는 함수
Task pop_task(TaskQueue *queue);

// 작업 큐 삭제 함수
void destroy_task_queue(TaskQueue *queue);

#endif // TASK_QUEUE_H
