#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include "Indicator_SWSPI.h"
#include "touch.h"
// #include "./demos/lv_demos.h" // Optional: Include only if you use LVGL demos
#include <time.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#define DIRECT_MODE
#define GFX_BL 45

#define GFX_DEV_DEVICE ESP32_S3_RGB

// Wi-Fi credentials
const char* ssid       = "nubia";
const char* password   = "22222222";

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

lv_obj_t *rects[4][4]; // Global array to store rectangle objects

TaskHandle_t lvglTaskHandle = NULL;

lv_color_t colors[] = {
    lv_color_hex(0xff0000), lv_color_hex(0x00ff00), lv_color_hex(0x0000ff),
    lv_color_hex(0xffff00), lv_color_hex(0xff00ff), lv_color_hex(0x00ffff),
    lv_color_hex(0x800000), lv_color_hex(0x008000), lv_color_hex(0x000080),
    lv_color_hex(0x808000), lv_color_hex(0x800080), lv_color_hex(0x008080),
    lv_color_hex(0xc0c0c0), lv_color_hex(0x808080), lv_color_hex(0xffa500),
    lv_color_hex(0xa52a2a)
};
int num_colors = sizeof(colors) / sizeof(colors[0]);

void update_rect_colors() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            lv_obj_set_style_bg_color(rects[i][j], colors[random(num_colors)], LV_PART_MAIN);
        }
    }
}

void lvglTask(void *pvParameters) {
    Serial.println("LVGL Task started...");

    lv_init();
    lv_tick_set_cb(millis);

    screenWidth = gfx->width();
    screenHeight = gfx->height();

    bufSize = screenWidth * screenHeight; // Single buffer size

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        Serial.println("PSRAM allocation failed! Trying DRAM...");
        disp_draw_buf = (lv_color_t *)malloc(bufSize * sizeof(lv_color_t));
        if (!disp_draw_buf) {
            Serial.println("DRAM allocation failed! Restarting...");
            ESP.restart();
        }
    }

    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Create 4x4 colored rectangles
    int rows = 4;
    int cols = 4;
    lv_coord_t cell_width = screenWidth / cols;
    lv_coord_t cell_height = screenHeight / rows;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            rects[i][j] = lv_obj_create(lv_scr_act());
            lv_obj_set_size(rects[i][j], cell_width, cell_height);
            lv_obj_set_pos(rects[i][j], j * cell_width, i * cell_height);
            lv_obj_set_style_bg_color(rects[i][j], colors[(i * cols + j) % num_colors], LV_PART_MAIN);
            lv_obj_set_style_border_width(rects[i][j], 0, LV_PART_MAIN);
        }
    }

    Serial.println("LVGL UI initialized.");

    uint32_t last_lv_task_time = millis();

    while (1) {
        lv_task_handler(); // Handle LVGL tasks FIRST

        // Update the display at a controlled rate (e.g., every 30ms) AFTER lv_task_handler()
        if (millis() - last_lv_task_time >= 30) {
            last_lv_task_time = millis();
            if (disp_draw_buf) { // Check if disp_draw_buf is valid
                gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
            }
        }

        vTaskDelay(1); // Short delay; LVGL timing is handled by the if statement above
    }
}
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    // In DIRECT_MODE, we do the flushing in the main loop, so this function just signals ready
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    static bool pressed = false;

    if (touch_has_signal()) {
        if (touch_touched()) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
            if (!pressed) {
                update_rect_colors();
                pressed = true;
            }
        } else { // Simplified: No need for separate released check
            data->state = LV_INDEV_STATE_RELEASED;
            pressed = false;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        pressed = false;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32-S3 LVGL Touch Example");

    WiFi.begin(ssid, password); // Connect to Wi-Fi (optional, for your NTP example)
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Configure time (optional)

    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
        for(;;); // Stop here if the display doesn't initialize
    }
    gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH); // Turn on backlight
#endif

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());

    // Create the LVGL task with sufficient stack size
    xTaskCreatePinnedToCore(lvglTask, "LVGL Task", 10240, NULL, 1, &lvglTaskHandle, 1);

    Serial.println("Setup done");
}

void loop() {
    vTaskDelay(portMAX_DELAY); // Keep the main loop empty
}