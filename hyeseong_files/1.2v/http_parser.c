#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void http_parsing(char *request, HttpRequest *http_request) {
            
    // 요청 라인 파싱
    char *line = strtok(request, "\r\n");
    if (line) {
        sscanf(line, "%s %s %s", http_request->method, http_request->path, http_request->version);
    }

    // 헤더 파싱
    http_request->header_count = 0;
    while ((line = strtok(NULL, "\r\n")) && strcmp(line, "") != 0) {
        char header_name[128], header_value[256];
        sscanf(line, "%[^:]: %[^\r\n]", header_name, header_value);

        // 저장
        strncpy(http_request->headers[http_request->header_count][0], header_name, sizeof(header_name) - 1);
        strncpy(http_request->headers[http_request->header_count][1], header_value, sizeof(header_value) - 1);
        http_request->header_count++;
    }

    // 본문 파싱
    http_request->body = NULL;
    const char *content_length_key = "Content-Length";
    int content_length = 0;

    // Content-Length 확인
    for (int i = 0; i < http_request->header_count; i++) {
        if (strcasecmp(http_request->headers[i][0], content_length_key) == 0) {
            content_length = atoi(http_request->headers[i][1]);
            break;
        }
    }

    // 본문이 있으면 메모리를 할당하고 복사
    if (content_length > 0) {
        line = strtok(NULL, ""); // 나머지 문자열 읽기 (본문)
        if (line) {
            http_request->body = malloc(content_length + 1);
            strncpy(http_request->body, line, content_length);
            http_request->body[content_length] = '\0'; // 널 추가
        }
    }
}
