#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <sys/time.h>
#include "canvas.h"
#include <sys/stat.h> // mkdir
#include <sys/types.h> // mode_t

// JSON 파일로 캔버스 저장
void save_canvas_as_json(Canvas *canvas) {
    
    // 타이밍 시작
    clock_t start = clock();

    // 파일 이름 생성
    char *filename = "save.json";

    // JSON 객체 생성
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "width", canvas->canvas_width);
    cJSON_AddNumberToObject(root, "height", canvas->canvas_height);

    // 픽셀 데이터 배열 생성
    cJSON *pixels = cJSON_CreateArray();
    for (int i = 0; i < canvas->canvas_width * canvas->canvas_height; i++) {
        cJSON_AddItemToArray(pixels, cJSON_CreateString(canvas->pixels[i].color));
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

    // 타이밍 끝
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("JSON 저장 소요 시간: %f 초\n", time_taken);

    // 메모리 해제
    free(json_string);
    cJSON_Delete(root);
}

char *trans_canvas_as_json(Canvas *canvas) {
    // 타이밍 시작
    clock_t start = clock();

    // 최상위 JSON 객체 생성
    cJSON *root = cJSON_CreateObject();
    cJSON *init = cJSON_CreateObject(); // "init" 객체 생성
    cJSON_AddItemToObject(root, "init", init);

    // Canvas 데이터 추가
    cJSON_AddNumberToObject(init, "width", canvas->canvas_width);
    cJSON_AddNumberToObject(init, "height", canvas->canvas_height);

    // 픽셀 데이터 배열 생성
    cJSON *pixels = cJSON_CreateArray();
    for (int i = 0; i < canvas->canvas_width * canvas->canvas_height; i++) {
        cJSON_AddItemToArray(pixels, cJSON_CreateString(canvas->pixels[i].color));
    }

    // 픽셀 데이터를 JSON에 추가
    cJSON_AddItemToObject(init, "pixels", pixels);

    // JSON 데이터를 파일로 저장
    char *json_string = cJSON_Print(root);

    // 타이밍 끝        
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("변환 저장 소요 시간: %f 초\n", time_taken);

    cJSON_Delete(root);
    return json_string;
}
