#include "context.h"
#include "thread_pool.h"
#include "server.h"
#include "canvas.h"
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <cjson/cJSON.h>

static int count = 0;

int main(){
    // Context 구조체 초기화
    Context ctx;

    init_server(&ctx.sm);                  // 서버 초기화
    initClientManager(&ctx.cm);            // 클라이언트 관리 초기화
    init_task_queue(&ctx.queue);           // 작업 큐 초기화

    canvas_init(&ctx.canvas);              // 캔버스 초기화

    // 스레드 풀 및 브로드캐스트 스레드 초기화
    ThreadPool pool;
    init_thread_pool(&pool, &ctx);

    // 이벤트 루프
    while (1) {
        int num_events = epoll_wait(ctx.sm.epoll_fd, ctx.sm.events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait failed");
            continue;
        }

        for (int i = 0; i < num_events; i++) 
        {
            if (ctx.sm.events[i].data.fd == ctx.sm.server_socket) 
            {
                // 서버 소켓에서 새로운 클라이언트 연결 처리
                Task task = {ctx.sm.server_socket, TASK_NEW_CLIENT, ""};
                push_task(&ctx.queue, task);
                count++;
            } 
            else 
            {
                // 클라이언트 소켓에서 데이터 수신
                char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));
                int client_socket = ctx.sm.events[i].data.fd;
                ssize_t n = recv(client_socket, buffer, BUFFER_SIZE, 0);
                count++;

                if (n <= 0) {
                    if (n == 0) printf("Client disconnected\n");
                    else printf("Client Error\n");
                    removeClient(&ctx.cm, client_socket);
                    epoll_ctl(ctx.sm.epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                    free(buffer);
                }
                else
                {
                    buffer[n] = '\0';  // NULL 종결자 추가
                    Task task = {client_socket, TASK_PIXEL_UPDATE, buffer};
                    push_task(&ctx.queue, task);
                }

                printf("Count : %d\n", count);
            }
        }
    }


    // 리소스 정리
    close_server(&ctx.sm);
    destroy_thread_pool(&pool);

    return 0;
}
