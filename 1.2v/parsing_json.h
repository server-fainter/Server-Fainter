
#ifndef PARSING_JSON_H
#define PARSING_JSON_H

#include "canvas.h"

void process_json(Canvas *canvas, const char *buffer, size_t length);
Pixel *parse_pixel_json(const char *json_str);
bool is_valid_hex_color(const char *color);
bool is_valid_coordinate(int x, int y, int width, int height);

#endif //PARSING_JSON_H
