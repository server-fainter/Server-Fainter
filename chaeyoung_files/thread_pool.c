#include "thread_pool.h"
#include "context.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

// 워커 스레드, 작업 큐에서 작업을 가져와 처리
static void *worker_thread(void *arg) {
    Context *ctx = (Context *)arg; // Context 구조체 참조
    TaskQueue *queue = &ctx->queue;

    while (1) {
        Task task = pop_task(queue); // 작업 큐에서 작업을 가져옴

        // 작업 유형에 따라 처리
        switch (task.type) 
        {
        case 0: {
            // 클라이언트 연결 처리
            accept_new_client(&ctx->sm);
            add_client(&ctx->cm, task.fd);      
            break;
        }
        case 1: {
            // 클라이언트 요청 처리
            char buffer[1024];
            ssize_t bytes_read = recv(task.fd, buffer, sizeof(buffer), 0);

            if (bytes_read <= 0) {
                // 데이터 수신 실패 처리
                perror("recv failed"); 
            } 
            else {
                // 받은 데이터에 널 추가
                buffer[bytes_read] = '\0'; 
                printf("Received from client %d: %s\n", task.fd, buffer); // 데이터 출력
                send(task.fd, "Nice", bytes_read, 0); // 간단한 응답 전송
            }
            break;
        }
        default: // 알 수 없는 작업 유형 처리
            fprintf(stderr, "Unknown task type: %d\n", task.type); 
            break;
        }
    }

    return NULL; // 스레드 종료
}

// 스레드 풀 초기화, 지정된 크기만큼 스레드를 생성
void init_thread_pool(ThreadPool *pool, Context *ctx) {
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, ctx);
    }
}

// 스레드 풀 삭제, 모든 스레드를 종료하고 리소스를 해제
void destroy_thread_pool(ThreadPool *pool) {
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_cancel(pool->threads[i]);           
        pthread_join(pool->threads[i], NULL);        
    }
}
