#include "context.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>

static int count = 0;

int main() {

     // Context 구조체 초기화
     Context *ctx = malloc(sizeof(Context));
     init_context(ctx);


     // 이벤트 루프
     while (1) {
         int num_events = epoll_wait(ctx->cm->epoll_fd, ctx->cm->events, EVENTS_SIZE, -1);
         if (num_events == -1) {
             perror("epoll_wait failed");
             continue;
         }

         for (int i = 0; i < num_events; i++)
         {
             if (ctx->cm->events[i].data.fd == ctx->cm->server_socket)
             {
                 // 새로운 클라이언트 연결 처리
                 Task task = { TASK_NEW_CLIENT, "" };
                 push_task(ctx->cm->queue, task);

                 count++;
             }
             else
             {

                 // 클라이언트 소켓에서 데이터 수신
                 char *buffer = (char *)malloc(REQUEST_BUFFER_SIZE * sizeof(char));
                 int client_socket = ctx->cm->events[i].data.fd;
                 ssize_t n = recv(client_socket, buffer, REQUEST_BUFFER_SIZE, 0);

                 if (n <= 0) {
                     if (n == 0) printf("Client disconnected\n");
                     else printf("Client Error\n");
                     removeClient(ctx->cm, client_socket);
                     epoll_ctl(ctx->cm->epoll_fd, EPOLL_CTL_DEL, client_socket, NULL);
                     free(buffer);
                 }
                 else
                 {
                     buffer[n] = '\0';  // NULL 종결자 추가
                     Task task = {TASK_PIXEL_UPDATE, buffer};
                     push_task(ctx->canvas->queue, task);
                 }

                 count++;

                 printf("Count : %d\n", count);
             }
         }
     }

    //리소스 정리
    destroyClientManger(ctx->cm);

    return 0;
}
