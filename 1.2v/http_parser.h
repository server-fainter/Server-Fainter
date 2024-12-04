#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

// HTTP 요청 정보를 저장하는 구조체
typedef struct {
    char method[16];
    char path[256];
    char version[16];
    char headers[32][2][256]; // 최대 32개의 헤더, 각 헤더는 [이름, 값] 쌍
    int header_count;
    char *body; // 본문 데이터를 저장할 포인터
} HttpRequest;

void http_parsing(char *request, HttpRequest *http_request);

#endif // HTTP_PARSER_H
