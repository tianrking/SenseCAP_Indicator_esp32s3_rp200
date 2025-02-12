#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include "Indicator_SWSPI.h"
#include "touch.h"
#include "./demos/lv_demos.h"
// #include "ui_dev.h" // 如果你没有用到 ui_dev.h 里的东西，可以注释掉
#include <time.h>
#include <WiFi.h>

#define DIRECT_MODE
#define GFX_BL 45

#define GFX_DEV_DEVICE ESP32_S3_RGB

// Wi-Fi credentials
const char* ssid       = "nubia";  // 你的 WiFi SSID
const char* password   = "22222222"; // 你的 WiFi 密码

// NTP server and time zone settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800; // +8 timezone
const int   daylightOffset_sec = 0;

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

// 时钟 label 对象
lv_obj_t *time_label;
lv_obj_t *bg_obj; // 背景框对象 (可选)


TaskHandle_t lvglTaskHandle = NULL;

void update_clock();  // 前向声明

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

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
    }
    if (!disp_draw_buf) {
        Serial.println("LVGL disp_draw_buf allocation failed!");
        vTaskDelete(NULL);
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
#ifdef DIRECT_MODE
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
#else
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

     // 可选：创建背景框
    bg_obj = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg_obj, 220, 80); // 根据字体大小调整
    lv_obj_center(bg_obj);
    lv_obj_set_style_bg_color(bg_obj, lv_color_hex(0x333333), LV_PART_MAIN); // 深灰色背景
    lv_obj_set_style_radius(bg_obj, 15, LV_PART_MAIN);         // 圆角
    lv_obj_set_style_border_width(bg_obj, 0, LV_PART_MAIN);   // 无边框
    lv_obj_set_style_pad_all(bg_obj, 10, LV_PART_MAIN);       // 内边距

    // 创建数字时钟的 label
    time_label = lv_label_create(bg_obj); // 将 label 放在背景框上
    // lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, LV_PART_MAIN); // 使用大字体
    // lv_obj_set_style_text_font(time_label, &lv_font_dejavu_16_persian_hebrew, LV_PART_MAIN);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, LV_PART_MAIN);

    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // 亮白色文本
    lv_obj_center(time_label);
    


    update_clock(); // 初始更新

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

        update_clock();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒更新
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

void update_clock() {
    time_t now;
    struct tm timeinfo;
    char time_str[9]; // HH:MM:SS\0

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo); // 格式化时间

    lv_label_set_text(time_label, time_str); // 更新 label 的文本
}

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32-S3 LVGL Digital Clock Example");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());

    xTaskCreatePinnedToCore(lvglTask, "LVGL Task", 8192, NULL, 1, &lvglTaskHandle, 1);

    Serial.println("Setup done");
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}