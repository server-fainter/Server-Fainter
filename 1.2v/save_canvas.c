#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <sys/time.h>
#include "canvas.h"
#include <sys/stat.h> // mkdir
#include <sys/types.h> // mode_t

// JSON 파일로 캔버스 저장
void save_canvas_as_json(Canvas *canvas, struct timeval time_val) {
    
    // 디렉토리 경로
    const char *directory = "save";
    
    // 디렉토리 생성 (존재하지 않으면 생성)
    struct stat st = {0};
    if (stat(directory, &st) == -1) {
        mkdir(directory, 0755); // 읽기, 쓰기, 실행 권한
    }

    // 파일 이름 생성
    char filename[256];
    time_t raw_time = time_val.tv_sec; 
    struct tm *t = localtime(&raw_time);

    snprintf(filename, sizeof(filename),
             "%s/canvas_%04d%02d%02d_%02d%02d%02d_%06ld.json",
             directory,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec,
             time_val.tv_usec);

    // JSON 객체 생성
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "width", canvas->canvas_width);
    cJSON_AddNumberToObject(root, "height", canvas->canvas_height);

    // 픽셀 데이터 배열 생성
    cJSON *pixels = cJSON_CreateArray();
    for (int i = 0; i < canvas->canvas_width * canvas->canvas_height; i++) {
        cJSON_AddItemToArray(pixels, cJSON_CreateNumber(canvas->pixels[i].color));
    }

    // 픽셀 데이터를 JSON에 추가
    cJSON_AddItemToObject(root, "pixels", pixels);

    // JSON 데이터를 파일로 저장
    char *json_string = cJSON_Print(root);
    FILE *file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%s", json_string);
        fclose(file);
        printf("JSON 파일 저장 완료: %s\n", filename);
    } else {
        perror("JSON 파일 저장 실패");
    }

    // 메모리 해제
    free(json_string);
    cJSON_Delete(root);
}