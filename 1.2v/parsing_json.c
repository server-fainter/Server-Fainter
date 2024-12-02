#include "parsing_json.h"
#include <cjson/cJSON.h>

// 유효한 좌표인지 확인
bool is_valid_coordinate(int x, int y, int width, int height) {
    return (x >= 0 && x < width && y >= 0 && y < height);
}

// 유효한 HEX 색상인지 확인
bool is_valid_hex_color(const char *color) {
    if (color[0] != '#' || strlen(color) != 7) {
        return false;
    }
    for (int i = 1; i < 7; i++) {
        if (!((color[i] >= '0' && color[i] <= '9') ||
              (color[i] >= 'A' && color[i] <= 'F') ||
              (color[i] >= 'a' && color[i] <= 'f'))) {
            return false;
              }
    }
    return true;
}

// JSON 데이터를 파싱하여 Pixel 구조체로 변환
Pixel *parse_pixel_json(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        fprintf(stderr, "Invalid JSON: %s\n", json_str);
        return NULL;
    }

    // "pixel" 객체 찾기
    cJSON *pixel_item = cJSON_GetObjectItem(json, "pixel");
    if (!cJSON_IsObject(pixel_item)) {
        fprintf(stderr, "Invalid pixel object in JSON: %s\n", json_str);
        cJSON_Delete(json);
        return NULL;
    }

    // "pixel" 객체 내의 "x", "y", "color" 찾기
    cJSON *x_item = cJSON_GetObjectItem(pixel_item, "x");
    cJSON *y_item = cJSON_GetObjectItem(pixel_item, "y");
    cJSON *color_item = cJSON_GetObjectItem(pixel_item, "color");

    if (!cJSON_IsNumber(x_item) || !cJSON_IsNumber(y_item) || !cJSON_IsString(color_item)) {
        fprintf(stderr, "Invalid pixel data in JSON: %s\n", json_str);
        cJSON_Delete(json);
        return NULL;
    }

    Pixel *parsed_pixel = (Pixel *)malloc(sizeof(Pixel));
    if (parsed_pixel == NULL) {
        fprintf(stderr, "Failed to allocate memory for Pixel.\n");
        cJSON_Delete(json);
        return NULL;
    }
    parsed_pixel->x = x_item->valueint;
    parsed_pixel->y = y_item->valueint;
    strncpy(parsed_pixel->color, color_item->valuestring, 8);

    cJSON_Delete(json);
    return parsed_pixel;
}

void process_json(Canvas *canvas, const char *buffer, size_t length) {

    size_t start = 0;  // JSON 객체 시작 위치
    int brace_count = 0; // 중괄호 개수 추적

    for (size_t i = 0; i < length; i++) {
        if (buffer[i] == '{') {
            // 열린 중괄호를 만나면 brace_count 증가
            if (brace_count == 0) {
                start = i; // JSON 시작 위치 기록
            }
            brace_count++;
        } else if (buffer[i] == '}') {
            // 닫힌 중괄호를 만나면 brace_count 감소
            brace_count--;

            // 완전한 JSON 객체를 감지
            if (brace_count == 0) {
                size_t json_obj_length = i - start + 1;

                // JSON 데이터 추출
                char *json_data = (char *)malloc(json_obj_length + 1);
                strncpy(json_data, buffer + start, json_obj_length);
                json_data[json_obj_length] = '\0';

                // Pixel 데이터 파싱
                Pixel *pixel = parse_pixel_json(json_data);
                free(json_data);

                if (pixel) {
                    if (is_valid_coordinate(pixel->x, pixel->y, canvas->canvas_width, canvas->canvas_height) &&
                        is_valid_hex_color(pixel->color)) {

                        // printf("Update Pixel: x=%d, y=%d, color=%s\n", pixel->x, pixel->y, pixel->color);

                        // 픽셀 업데이트 처리
                        int index = pixel->y * canvas->canvas_width + pixel->x;
                        strcpy(canvas->pixels[index].color, pixel->color);

                        // 수정된 픽셀 해시 맵에 추가
                        int key = index;
                        ModifiedPixel *p;
                        HASH_FIND_INT(canvas->modified_pixels, &key, p);
                        if (p == NULL) {
                            p = (ModifiedPixel *)malloc(sizeof(ModifiedPixel));
                            p->key = key;
                            strcpy(p->color, pixel->color);
                            HASH_ADD_INT(canvas->modified_pixels, key, p);
                        } else {
                            // 이미 존재하면 색상 업데이트
                            strcpy(p->color, pixel->color);
                        }
                    } else {
                        fprintf(stderr, "Invalid Pixel: x=%d, y=%d, color=%s\n", pixel->x, pixel->y, pixel->color);
                    }
                    free(pixel);
                }
            }
        }
    }
}