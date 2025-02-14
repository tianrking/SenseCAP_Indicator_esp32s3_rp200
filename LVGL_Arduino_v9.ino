#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include "Indicator_SWSPI.h"
// #include "touch.h" // Removed touch
#include <time.h>
#include <WiFi.h>

#define DIRECT_MODE
#define GFX_BL 45
#define GFX_DEV_DEVICE ESP32_S3_RGB

// Wi-Fi credentials
const char* ssid = "nubia";  // REPLACE
const char* password = "22222222"; // REPLACE

// NTP server and time zone settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // Base offset
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

TaskHandle_t lvglTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t timeTaskHandle = NULL;

// Global variables
const char *cityNames[] = {"Barcelona", "Istanbul", "Tokyo", "New York", "Hong Kong"};
long timeOffsets[] = {7200, 10800, 32400, -18000, 28800};
lv_obj_t *cityLabels[5];
lv_obj_t *cityNameLabels[5];
lv_obj_t *cityContainers[4]; // Top city containers
lv_obj_t *topContainer;
lv_obj_t *bottomContainer;
// int cityOrder[5] = {0, 1, 2, 3, 4}; // No longer needed for display-only
// No need for city_click_event_handler or rearrangeGrid

void timeTask(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void lvglTask(void *pvParameters) {
    Serial.println("LVGL Task started...");

    lv_init();
    lv_tick_set_cb(millis);

    screenWidth = gfx->width();
    screenHeight = gfx->height();
    bufSize = screenWidth * screenHeight;

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

    // NO TOUCH INPUT NEEDED
    // lv_indev_t *indev = lv_indev_create();
    // lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    // lv_indev_set_read_cb(indev, my_touchpad_read);

    // --- Background ---
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x222222), LV_PART_MAIN);

    // --- Top Section (4 Cities) ---
    topContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(topContainer, screenWidth, screenHeight / 2);
    lv_obj_set_layout(topContainer, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(topContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(topContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(topContainer, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_clear_flag(topContainer, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t col_dsc[] = {screenWidth / 2, screenWidth / 2, LV_GRID_TEMPLATE_LAST};
    lv_coord_t row_dsc[] = {screenHeight / 4, screenHeight / 4, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(topContainer, col_dsc, row_dsc);

    for (int i = 0; i < 4; i++) {
        cityContainers[i] = lv_obj_create(topContainer);
        lv_obj_set_size(cityContainers[i], screenWidth / 2, screenHeight / 4);
        lv_obj_set_layout(cityContainers[i], LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(cityContainers[i], LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cityContainers[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(cityContainers[i], 2, LV_PART_MAIN);
        lv_obj_set_style_border_width(cityContainers[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(cityContainers[i], lv_color_hex(0x555555), LV_PART_MAIN);
        lv_obj_set_style_bg_color(cityContainers[i], lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_clear_flag(cityContainers[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_grid_cell(cityContainers[i], LV_GRID_ALIGN_STRETCH, i % 2, 1,
                             LV_GRID_ALIGN_STRETCH, i / 2, 1); // Initial grid cell setting

        cityLabels[i] = lv_label_create(cityContainers[i]);
        lv_obj_set_style_text_font(cityLabels[i], &lv_font_montserrat_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(cityLabels[i], lv_color_hex(0xdddddd), LV_PART_MAIN);
        lv_label_set_text(cityLabels[i], "00:00:00");

        cityNameLabels[i] = lv_label_create(cityContainers[i]);
        lv_obj_set_style_text_font(cityNameLabels[i], &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(cityNameLabels[i], lv_color_hex(0x888888), LV_PART_MAIN);
        lv_label_set_text(cityNameLabels[i], cityNames[i]);

        // NO EVENT HANDLERS OR CLICKABILITY
        // lv_obj_add_event_cb(cityContainers[i], city_click_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)cityOrder[i]);
        // lv_obj_add_flag(cityContainers[i], LV_OBJ_FLAG_CLICKABLE);
    }

    // --- Bottom Section (Initially Hong Kong) ---
    bottomContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bottomContainer, screenWidth, screenHeight / 2);
    lv_obj_set_align(bottomContainer, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_layout(bottomContainer, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bottomContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bottomContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bottomContainer, LV_OBJ_FLAG_SCROLLABLE);

    cityLabels[4] = lv_label_create(bottomContainer);
    lv_obj_set_style_text_font(cityLabels[4], &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(cityLabels[4], lv_color_hex(0xADD8E6), LV_PART_MAIN);
    lv_label_set_text(cityLabels[4], "00:00:00");

    cityNameLabels[4] = lv_label_create(bottomContainer);
    lv_obj_set_style_text_font(cityNameLabels[4], &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(cityNameLabels[4], lv_color_hex(0x888888), LV_PART_MAIN);
    lv_label_set_text(cityNameLabels[4], cityNames[4]);

    // NO EVENT HANDLERS OR CLICKABILITY
    // lv_obj_add_event_cb(bottomContainer, city_click_event_handler, LV_EVENT_CLICKED, (void*)4);
    // lv_obj_add_flag(bottomContainer, LV_OBJ_FLAG_CLICKABLE);

    Serial.println("LVGL UI initialized.");
    // No need to call rearrangeGrid()

    uint32_t last_lv_task_time = millis();
    while (1) {
        lv_task_handler();
        if (millis() - last_lv_task_time >= 500) {
            last_lv_task_time = millis();

            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                // Update all city times (no cityOrder needed)
                for (int i = 0; i < 5; i++) {
                    struct tm cityTime = timeinfo;
                    cityTime.tm_hour += (timeOffsets[i] / 3600); // Use 'i' directly
                    cityTime.tm_min += (timeOffsets[i] % 3600) / 60;
                    mktime(&cityTime); // Normalize
                    char timeString[9];
                    strftime(timeString, sizeof(timeString), "%H:%M:%S", &cityTime);
                    lv_label_set_text(cityLabels[i], timeString);
                    lv_label_set_text(cityNameLabels[i], cityNames[i]); // Use 'i' directly

                }
            } else {
                Serial.println("Failed to obtain time");
            }

            if (disp_draw_buf) {
                gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
            }
        }
        vTaskDelay(1);
    }
}

// NO CLICK HANDLER NEEDED
// static void city_click_event_handler(lv_event_t *e) { ... }

// NO GRID RE-ARRANGEMENT NEEDED
// void rearrangeGrid() { ... }


void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    lv_disp_flush_ready(disp);
}

// NO TOUCHPAD READ NEEDED
// void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) { ... }

void wifiTask(void *pvParameters) {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("Time configured");

    xTaskCreatePinnedToCore(timeTask, "Time Update Task", 2048, NULL, 2, &timeTaskHandle, 1);

    vTaskDelete(NULL);
}

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32-S3 LVGL Multi-Timezone Clock");

    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
        for (;;);
    }
    gfx->fillScreen(RGB565_BLACK);

#ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
#endif

    // touch_init(gfx->width(), gfx->height(), gfx->getRotation()); // Removed touch

    xTaskCreatePinnedToCore(wifiTask, "WiFi Task", 4096, NULL, 1, &wifiTaskHandle, 1);
    xTaskCreatePinnedToCore(lvglTask, "LVGL Task", 8192, NULL, 1, &lvglTaskHandle, 1);

    Serial.println("Setup done");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}