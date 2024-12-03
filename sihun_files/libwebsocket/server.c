#include <libwebsockets.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 8080
#define WIDTH 100
#define HEIGHT 100

static uint8_t canvas[WIDTH][HEIGHT];
static pthread_mutex_t canvas_mutex;

// 클라이언트 목록 관리 구조체
typedef struct per_session_data {
    struct lws *wsi;
    struct per_session_data *prev;
    struct per_session_data *next;
    unsigned char *buf; // 버퍼 포인터로 변경
    size_t buf_size;    // 버퍼 크기
    size_t len;         // 메시지 길이
} per_session_data_t;

// 연결된 모든 클라이언트의 리스트
static per_session_data_t *clients_list = NULL;
static pthread_mutex_t clients_mutex;

// 모든 연결된 클라이언트들에게 메시지 브로드캐스트
void broadcast_to_clients(const char *protocol_name, unsigned char *message, size_t len) {
    per_session_data_t *psd;

    pthread_mutex_lock(&clients_mutex);
    for (psd = clients_list; psd != NULL; psd = psd->next) {
        if (strcmp(lws_get_protocol(psd->wsi)->name, protocol_name) == 0) {
            // 버퍼 크기 확인 및 재할당
            if (psd->buf_size < LWS_PRE + len) {
                unsigned char *new_buf = realloc(psd->buf, LWS_PRE + len);
                if (!new_buf) {
                    fprintf(stderr, "Failed to reallocate memory for psd->buf\n");
                    continue;
                }
                psd->buf = new_buf;
                psd->buf_size = LWS_PRE + len;
            }
            // 메시지를 세션 데이터에 저장
            memcpy(&psd->buf[LWS_PRE], message, len);
            psd->len = len;

            // 쓰기 가능한 콜백 예약
            lws_callback_on_writable(psd->wsi);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

static int callback_rplace(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    per_session_data_t *psd = (per_session_data_t *)user;

    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: //0
            // 클라이언트 연결됨
            psd->wsi = wsi;
            printf("Client connected\n");

            // 버퍼 할당
            psd->buf_size = LWS_PRE + WIDTH * HEIGHT * 3;
            psd->buf = malloc(psd->buf_size);
            if (!psd->buf) {
                fprintf(stderr, "Failed to allocate memory for psd->buf\n");
                return -1;
            }

            // 클라이언트 리스트에 추가
            pthread_mutex_lock(&clients_mutex);
            psd->next = clients_list;
            psd->prev = NULL;
            if (clients_list)
                clients_list->prev = psd;
            clients_list = psd;
            pthread_mutex_unlock(&clients_mutex);

            // 현재 캔버스 상태 전송
            {
                pthread_mutex_lock(&canvas_mutex);
                size_t init_len = WIDTH * HEIGHT * 3;
                unsigned char *init_msg = malloc(init_len);
                if (!init_msg) {
                    pthread_mutex_unlock(&canvas_mutex);
                    fprintf(stderr, "Failed to allocate memory for init_msg\n");
                    return -1;
                }
                size_t index = 0;
                for (uint8_t y = 0; y < HEIGHT; y++) {
                    for (uint8_t x = 0; x < WIDTH; x++) {
                        init_msg[index++] = x;
                        init_msg[index++] = y;
                        init_msg[index++] = canvas[x][y];
                    }
                }
                pthread_mutex_unlock(&canvas_mutex);

                // 버퍼 크기 확인 및 재할당
                if (psd->buf_size < LWS_PRE + init_len) {
                    unsigned char *new_buf = realloc(psd->buf, LWS_PRE + init_len);
                    if (!new_buf) {
                        fprintf(stderr, "Failed to reallocate memory for psd->buf\n");
                        free(init_msg);
                        return -1;
                    }
                    psd->buf = new_buf;
                    psd->buf_size = LWS_PRE + init_len;
                }

                // 메시지 저장 및 전송 예약
                memcpy(&psd->buf[LWS_PRE], init_msg, init_len);
                psd->len = init_len;
                lws_callback_on_writable(psd->wsi);

                free(init_msg);
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            // 클라이언트로부터 픽셀 정보 수신
            if (len == 3) {
                uint8_t *data = (uint8_t *)in;
                uint8_t x = data[0];
                uint8_t y = data[1];
                uint8_t color = data[2];

                if (x < WIDTH && y < HEIGHT) {
                    pthread_mutex_lock(&canvas_mutex);
                    canvas[x][y] = color;
                    pthread_mutex_unlock(&canvas_mutex);

                    // 변경 사항을 브로드캐스트
                    unsigned char msg[3];
                    msg[0] = x;
                    msg[1] = y;
                    msg[2] = color;

                    broadcast_to_clients("rplace-protocol", msg, 3);
                }
            }
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (psd->len > 0) {
                int n = lws_write(wsi, &psd->buf[LWS_PRE], psd->len, LWS_WRITE_BINARY);
                if (n < (int)psd->len) {
                    fprintf(stderr, "ERROR %d writing to socket\n", n);
                    return -1;
                }
                psd->len = 0; // 전송 후 길이 초기화
            }
            break;

        case LWS_CALLBACK_CLOSED:
            // 클라이언트 연결 종료
            printf("Client disconnected\n");

            // 버퍼 해제
            if (psd->buf) {
                free(psd->buf);
                psd->buf = NULL;
            }

            // 클라이언트 리스트에서 제거
            pthread_mutex_lock(&clients_mutex);
            if (psd->prev)
                psd->prev->next = psd->next;
            else
                clients_list = psd->next;
            if (psd->next)
                psd->next->prev = psd->prev;
            pthread_mutex_unlock(&clients_mutex);
            break;

        default:
            break;
    }
    return 0;
}

int main(void) {
    struct lws_context_creation_info info;
    struct lws_protocols protocols[] = {
        {
            "rplace-protocol",
            callback_rplace,
            sizeof(per_session_data_t),
            1024,
        },
        { NULL, NULL, 0, 0 } // NULL로 끝내야 함
    };

    memset(&info, 0, sizeof(info));
    info.port = PORT;
    info.protocols = protocols;

    pthread_mutex_init(&canvas_mutex, NULL);
    pthread_mutex_init(&clients_mutex, NULL);

    // 캔버스 초기화 (필요한 경우)
    memset(canvas, 0, sizeof(canvas));

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "lws_create_context failed\n");
        return -1;
    }

    // 메인 서버 루프
    while (1) {
        lws_service(context, 1000);
    }

    pthread_mutex_destroy(&canvas_mutex);
    pthread_mutex_destroy(&clients_mutex);
    lws_context_destroy(context);

    return 0;
}
