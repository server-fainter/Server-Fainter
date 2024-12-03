#include "context.h"
#include "task_queue.h"
#include "thread_pool.h"
#include "server.h"
#include "clientmanager.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/epoll.h>

// HTTP 요청 처리 함수 선언
void handle_http_request(int client_socket, const char *buffer);  // 수정된 선언


int main() {
    // Context 구조체 초기화
    Context ctx;

    // 서버 매니저 초기화 (서버 소켓 설정 포함)
    init_server(&ctx.sm); 

    // 클라이언트 매니저 초기화
    init_clients(&ctx.cm); 

    // 작업 큐 초기화
    init_task_queue(&ctx.queue);

    // 스레드 풀 초기화
    ThreadPool pool;
    init_thread_pool(&pool, &ctx);
    
    // // Epoll을 사용한 이벤트 처리 루프
    // while (1) {
    //     // epoll_wait로 이벤트 감지 (무한 대기)
    //     int num_events = epoll_wait(ctx.sm.epoll_fd, ctx.sm.events, MAX_EVENTS, -1);
    //     if (num_events == -1) {
    //         // epoll_wait 호출 실패 시 에러 출력
    //         perror("epoll_wait failed");
    //         continue;
    //     }

    //     // 감지된 이벤트들을 하나씩 처리
    //     for (int i = 0; i < num_events; i++) {
    //         if (ctx.sm.events[i].data.fd == ctx.sm.server_socket) 
    //         {
    //             // 서버 소켓에서 이벤트 발생 (새로운 클라이언트 연결 요청)
    //             // Accept 작업을 처리하는 태스크 생성 후 작업 큐에 추가
    //             Task task = {ctx.sm.server_socket, 0};
    //             push_task(&ctx.queue, task);
    //         }
    //         else 
    //         {
    //             // 클라이언트 소켓에서 이벤트 발생 (데이터 수신/처리 요청)
    //             // 클라이언트 요청 처리 작업을 태스크 큐에 추가
    //             Task task = {ctx.sm.events[i].data.fd, 1};
    //             push_task(&ctx.queue, task);
    //         }
    //     }
    // }

    // 서버 이벤트 루프 실행
    server_event_loop(&ctx.sm);

    // 이벤트 루프 종료 후 리소스 정리
    close_server(&ctx.sm);           // 서버 소켓 및 관련 리소스 해제
    destroy_thread_pool(&pool);      // 스레드 풀 리소스 해제

    return 0;
}