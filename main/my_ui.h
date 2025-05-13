// my_ui.h
#ifndef MY_UI_H
#define MY_UI_H

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480
#define TILE_SIZE 256

extern volatile bool g_wifi_is_connected;

// These macros will be used for INITIAL tile coordinates
// #define INITIAL_TILE_X 3331
// #define INITIAL_TILE_Y 1616
// #define INITIAL_TILE_Z 12

// x=25869, y=13309, z=15
// #define INITIAL_TILE_X 3232
// #define INITIAL_TILE_Y 1662
// #define INITIAL_TILE_Z 12

#define INITIAL_TILE_X 202
#define INITIAL_TILE_Y 103
#define INITIAL_TILE_Z 8

#define TILE_URL_FORMAT "http://webrd01.is.autonavi.com/appmaptile?lang=zh_cn&size=1&scale=1&style=8&x=%d&y=%d&z=%d&key=d710f9736c71e3f8c5bb3ce7e1ac2116"

void my_ui_init_single_tile(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MY_UI_H