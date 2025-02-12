#include "ui_dev.h"
#include "Indicator_SWSPI.h"
#include <PCA95x5.h>
#include <Arduino_GFX_Library.h>
#include "touch.h"

// GFX_BL 定义
#define GFX_BL 45  // 设置背光引脚为 45，可以根据实际硬件修改

// 显示器初始化
Arduino_DataBus *bus = new Indicator_SWSPI(
    GFX_NOT_DEFINED /* DC */, PCA95x5::Port::P04 /* CS */,
    41 /* SCK */, 48 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
    10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
    15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 2 /* rotation */, false /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

lv_obj_t *local_time_label;

typedef struct {
    const char *city;
    const char *timezone;
    lv_obj_t *label;
    lv_obj_t *time_label; // 单独的时间标签
} CityTime;

CityTime cities[] = {
    {"Barcelona", "GMT+1", NULL, NULL},
    {"New York", "GMT-5", NULL, NULL},
    {"Tokyo", "GMT+9", NULL, NULL},
    {"Istanbul", "GMT+3", NULL, NULL},
};

// 更新香港时间的函数
static void update_hongkong_time(lv_timer_t *timer) {
    time_t now;
    struct tm timeinfo;
    char buf[64];

    setenv("TZ", "GMT+8", 1); // 香港时间
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    lv_label_set_text(local_time_label, buf);
    lv_obj_center(local_time_label); // 居中显示
}

// 更新其他城市时间的函数
static void update_all_city_time(lv_timer_t *timer) {
    time_t now;
    struct tm timeinfo;
    char buf[64];

    for (int i = 0; i < sizeof(cities) / sizeof(cities[0]); i++) {
        setenv("TZ", cities[i].timezone, 1);
        tzset();
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
        lv_label_set_text(cities[i].time_label, buf); // 更新时间标签
    }
}

// 刷新显示的函数
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
#ifndef DIRECT_MODE
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif // #ifndef DIRECT_MODE

    lv_disp_flush_ready(disp);
}

// 触摸板读取的函数
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (touch_has_signal()) {
        if (touch_touched()) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        } else if (touch_released()) {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// UI 初始化函数
void ui_init() {
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

    pinMode(GFX_BL, OUTPUT);    // 初始化背光控制
    digitalWrite(GFX_BL, HIGH); // 打开背光

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());
    
    lv_init();

    screenWidth = gfx->width();
    screenHeight = gfx->height();

    bufSize = screenWidth * 40;

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        Serial.println("LVGL disp_draw_buf allocate failed!");
        return;
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // 设置背景颜色为黑色，确保整个屏幕背景填充
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);

    local_time_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(local_time_label, &lv_font_montserrat_32, 0); // 使用更大的字体
    lv_obj_set_style_text_color(local_time_label, lv_color_hex(0xFFFFFF), 0); // 白色

    int label_height = 30;
    int padding = 10;
    int start_y = 20; // 调整位置以避免底部多余的空间

    lv_color_t colors[] = {
        lv_color_hex(0xFF9933), // Orange
        lv_color_hex(0x3399FF), // Blue
        lv_color_hex(0x99FF33), // Green
        lv_color_hex(0xFF3399)  // Pink
    };

    for (int i = 0; i < sizeof(cities) / sizeof(cities[0]); i++) {
        cities[i].label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(cities[i].label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cities[i].label, colors[i], 0);
        lv_label_set_text(cities[i].label, cities[i].city);
        lv_obj_set_width(cities[i].label, screenWidth / 2 - 20);
        lv_obj_set_pos(cities[i].label, (i % 2) * (screenWidth / 2) + 10, start_y + (i / 2) * (label_height + padding));

        cities[i].time_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(cities[i].time_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(cities[i].time_label, colors[i], 0);
        lv_obj_set_width(cities[i].time_label, screenWidth / 2 - 20);
        lv_obj_set_pos(cities[i].time_label, (i % 2) * (screenWidth / 2) + 10, start_y + (i / 2) * (label_height + padding) + 20);
    }

    lv_timer_create(update_hongkong_time, 1000, NULL);
    lv_timer_create(update_all_city_time, 1000, NULL);

    update_hongkong_time(NULL);
    update_all_city_time(NULL);
}

// lvgl_task 任务的实现
void lvgl_task(void *pvParameters) {
    while (1) {
        lv_task_handler();
        delay(5);
    }
}
