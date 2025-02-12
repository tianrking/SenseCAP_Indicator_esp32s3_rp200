#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include "Indicator_SWSPI.h"
#include "touch.h"
#include "./demos/lv_demos.h"

#define DIRECT_MODE
#define GFX_BL 45

#define GFX_DEV_DEVICE ESP32_S3_RGB

Arduino_DataBus *bus = new Indicator_SWSPI(
    GFX_NOT_DEFINED, PCA95x5::Port::P04,
    41, 48, GFX_NOT_DEFINED);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18, 17, 16, 21,
    4, 3, 2, 1, 0,
    10, 9, 8, 7, 6, 5,
    15, 14, 13, 12, 11,
    1, 10, 8, 50,
    1, 10, 8, 20);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480, 480, rgbpanel, 2, false,
    bus, GFX_NOT_DEFINED, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

uint32_t screenWidth, screenHeight, bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

TaskHandle_t lvglTaskHandle = NULL;

void lvglTask(void *pvParameters) {
    Serial.println("LVGL Task started...");

    lv_init();
    lv_tick_set_cb(millis);

    screenWidth = gfx->width();
    screenHeight = gfx->height();

#ifdef DIRECT_MODE
    bufSize = screenWidth * screenHeight;
#else
    bufSize = screenWidth * 40;
#endif

    // 优先使用 PSRAM 分配缓冲区
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
    }

    if (!disp_draw_buf) {
        Serial.println("LVGL disp_draw_buf allocation failed!");
        vTaskDelete(NULL); // 任务结束
    }

    // 创建 LVGL 显示
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);

#ifdef DIRECT_MODE
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
#else
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    // 初始化输入设备（触摸屏）
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // **在任务中初始化 UI**
    lv_demo_widgets();

    Serial.println("LVGL UI initialized.");

    while (1) {
        lv_task_handler();

    #ifdef DIRECT_MODE
        #if defined(CANVAS) || defined(RGB_PANEL)
            gfx->flush();
        #else
            gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
        #endif
    #else
        #ifdef CANVAS
            gfx->flush();
        #endif
    #endif

        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
#ifndef DIRECT_MODE
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, lv_area_get_width(area), lv_area_get_height(area));
#endif
    lv_disp_flush_ready(disp);
}

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

void setup() {
    Serial.begin(115200);
    Serial.println("SenseCap Indicator GFX LVGL_Arduino_v9 (ESP32S3) Example");

    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());

    // **创建 FreeRTOS 任务**
    xTaskCreatePinnedToCore(lvglTask, "LVGL Task", 8192, NULL, 1, &lvglTaskHandle, 1);

    Serial.println("Setup done");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
