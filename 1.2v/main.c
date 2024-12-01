#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include "client_manager.h"
#include "canvas.h"
#include "http_handler.h"

// static int count = 0;

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

         for (int i = 0; i < num_events; i++) {
            int fd = ctx->cm->events[i].data.fd;
         
            if (fd == ctx->cm->server_socket) {
                // 새로운 클라이언트 연결 처리
                Task task = {NULL, TASK_NEW_CLIENT, NULL};
                push_task(ctx->cm->queue, task);
                
            } 
            else {

                // 클라이언트로부터 데이터 수신
                Client *client = find_client(ctx->cm, fd);
                if (!client) {
                    fprintf(stderr, "클라이언트가 존재 하지 않음 Client : %d\n", fd);
                    continue;
                }

                char buffer[8192];
                ssize_t n = recv(fd, buffer, sizeof(buffer), 0);

                if (n <= 0) {
                    // 클라이언트 접속 종료
                    Task task = {client, TASK_CLIENT_CLOSE, ""};
                    push_task(ctx->cm->queue, task);
                }
                else {

                    // 수신된 데이터를 버퍼에 추가, 버퍼 크기 검사
                    if ((client->recv_buffer_len + n) < (REQUEST_BUFFER_SIZE * sizeof(char))){
                    
                        memcpy(client->recv_buffer + client->recv_buffer_len, buffer, n); // 버퍼에 남아 있는 곳에서 부터 copy
                        client->recv_buffer_len += n; // 길이 갱신 
                        client->recv_buffer[client->recv_buffer_len] = '\0'; // 널 추가
                    } 
                    else {
                        fprintf(stderr, "클라이언트 버퍼 오버플로우 Client : %d\n", fd);
                        // 에러 처리 (연결 종료) 
                        removeClient(ctx->cm, fd);
                        epoll_ctl(ctx->cm->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        continue;
                    }

                    // 클라이언트 상태에 따른 처리
                    if (client->state == CONNECTION_HANDSHAKE) {

                        Task task = {client, TASK_HTTP_REQUEST, ""};
                        push_task(ctx->cm->queue, task);
                        //printf("CONNECTION_HANDSHAKE\n");

                    } else if (client->state == CONNECTION_OPEN) {

                        Task task = {client, TASK_PIXEL_UPDATE, ""};
                        push_task(ctx->canvas->queue, task);
                        //printf("CONNECTION_OPEN\n");
                    }
                }
            }
        }
    }

    //리소스 정리
    destroyClientManger(ctx->cm);

    return 0;
}
