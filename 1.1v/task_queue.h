#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <pthread.h>

typedef enum {
    TASK_NEW_CLIENT,                // 새로운 클라이언트가 접속 요청하는 경우
    TASK_TEST_RECV_MESSAGE,         // 테스트용 단순한 메세지 버퍼 recv
    TASK_PIXEL_UPDATE,              // 픽셀 업데이트 작업
    TASK_SEND_STATIC_FILE,          // 정적 파일(HTML, CSS, JS, 캔버스 파일) send
    TASK_BROADCAST,                 // 브로드캐스팅(수정된 픽셀 정보)
}TaskType;

// 작업(Task) 구조체
typedef struct {
    TaskType type;     // 작업 유형
    void *data;        // 클라이언트로부터 받은 데이터 (예: JSON)
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
