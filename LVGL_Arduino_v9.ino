#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <PCA95x5.h>
#include "Indicator_SWSPI.h"
#include "touch.h"
#include <time.h>
#include <WiFi.h>

#define DIRECT_MODE
#define GFX_BL 45

#define GFX_DEV_DEVICE ESP32_S3_RGB

// Wi-Fi credentials
const char* ssid = "nubia";  // REPLACE WITH YOUR SSID
const char* password = "22222222"; // REPLACE WITH YOUR PASSWORD

// NTP server and time zone settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // Base offset - we'll adjust per city
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

// No global timeLabel needed - we're using cityLabels

TaskHandle_t lvglTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t timeTaskHandle = NULL;


// Time update task (gets base time, lvglTask applies offsets)
void timeTask(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second (display updates less often)
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

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // --- Background ---
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x222222), LV_PART_MAIN);

    // --- Top Section (4 Cities) ---
    lv_obj_t *topContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(topContainer, screenWidth, screenHeight / 2);
    lv_obj_set_layout(topContainer, LV_LAYOUT_GRID); // Grid layout
    lv_obj_set_style_pad_all(topContainer, 0, LV_PART_MAIN); // No padding *around* the grid
    lv_obj_set_style_border_width(topContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(topContainer, lv_color_hex(0x333333), LV_PART_MAIN);

    // Grid column and row templates (Corrected heights)
    lv_coord_t col_dsc[] = {screenWidth / 2, screenWidth / 2, LV_GRID_TEMPLATE_LAST};
    lv_coord_t row_dsc[] = {screenHeight / 4, screenHeight / 4, LV_GRID_TEMPLATE_LAST}; // Two rows per city
    lv_obj_set_grid_dsc_array(topContainer, col_dsc, row_dsc);

    // Disable scrolling on the top container
    lv_obj_clear_flag(topContainer, LV_OBJ_FLAG_SCROLLABLE);

    const char *cityNames[] = {"Barcelona", "Istanbul", "Tokyo", "New York"};
    long timeOffsets[] = {7200, 10800, 32400, -18000}; // Timezone offsets in seconds

    for (int i = 0; i < 4; i++) {
        // Create a container for *each* city (to hold time and name)
        lv_obj_t *cityContainer = lv_obj_create(topContainer);
        lv_obj_set_size(cityContainer, screenWidth / 2, screenHeight / 4);
        lv_obj_set_layout(cityContainer, LV_LAYOUT_FLEX);  // Use flex layout within each city container
        lv_obj_set_flex_flow(cityContainer, LV_FLEX_FLOW_COLUMN); // Stack items vertically
        lv_obj_set_flex_align(cityContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(cityContainer, 2, LV_PART_MAIN); // Small padding *inside* each cell
        lv_obj_set_style_border_width(cityContainer, 1, LV_PART_MAIN); // Add a border
        lv_obj_set_style_border_color(cityContainer, lv_color_hex(0x555555), LV_PART_MAIN); // Darker gray border
        lv_obj_set_style_bg_color(cityContainer, lv_color_hex(0x333333), LV_PART_MAIN);

        // Disable scrolling on city containers too
        lv_obj_clear_flag(cityContainer, LV_OBJ_FLAG_SCROLLABLE);

        // Place the city container in the grid
        lv_obj_set_grid_cell(cityContainer, LV_GRID_ALIGN_STRETCH, i % 2, 1,
                                           LV_GRID_ALIGN_STRETCH, i / 2, 1);

        // Time Label (inside cityContainer)
        lv_obj_t *cityLabel = lv_label_create(cityContainer);
        lv_obj_set_style_text_font(cityLabel, &lv_font_montserrat_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(cityLabel, lv_color_hex(0xdddddd), LV_PART_MAIN);
        lv_label_set_text(cityLabel, "00:00:00");

        // City Name Label (inside cityContainer)
        lv_obj_t *cityNameLabel = lv_label_create(cityContainer);
        lv_obj_set_style_text_font(cityNameLabel, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(cityNameLabel, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_label_set_text(cityNameLabel, cityNames[i]);
    }

    // --- Bottom Section (Hong Kong) ---
    lv_obj_t *hongKongLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(hongKongLabel, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(hongKongLabel, lv_color_hex(0xADD8E6), LV_PART_MAIN);
    lv_label_set_text(hongKongLabel, "00:00:00");
    lv_obj_align(hongKongLabel, LV_ALIGN_BOTTOM_MID, 0, -50);

    lv_obj_t *hongKongCityNameLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(hongKongCityNameLabel, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(hongKongCityNameLabel, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_label_set_text(hongKongCityNameLabel, "Hong Kong");
    lv_obj_align_to(hongKongCityNameLabel, hongKongLabel, LV_ALIGN_OUT_TOP_MID, 0, -5);

    Serial.println("LVGL UI initialized.");

    uint32_t last_lv_task_time = millis();
    while (1) {
        lv_task_handler();
        if (millis() - last_lv_task_time >= 500) {
            last_lv_task_time = millis();

            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {

                // Find the city containers and update their labels (iterate over children)
                lv_obj_t * child;
                uint32_t child_cnt = lv_obj_get_child_cnt(topContainer);
                for(uint32_t i = 0; i < child_cnt; i++) {
                    child = lv_obj_get_child(topContainer, i); // Get the city container
                    if (child) {
                      lv_obj_t * time_label = lv_obj_get_child(child, 0); // Time label is first child
                      if(time_label){
                        struct tm cityTime = timeinfo;
                        cityTime.tm_hour += (timeOffsets[i] / 3600);
                        cityTime.tm_min += (timeOffsets[i] % 3600) / 60;
                        mktime(&cityTime); // Normalize

                        char timeString[9];
                        strftime(timeString, sizeof(timeString), "%H:%M:%S", &cityTime);
                        lv_label_set_text(time_label, timeString); // Set time text
                      }
                    }
                }

                struct tm hkTime = timeinfo;
                hkTime.tm_hour += 8;  // Hong Kong: GMT+8
                mktime(&hkTime);
                char hkTimeString[9];
                strftime(hkTimeString, sizeof(hkTimeString), "%H:%M:%S", &hkTime);
                lv_label_set_text(hongKongLabel, hkTimeString);

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

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    static bool pressed = false;

    if (touch_has_signal()) {
        if (touch_touched()) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
            pressed = true;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
            pressed = false;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        pressed = false;
    }
}
void wifiTask(void *pvParameters) {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Base time (no offset)
    Serial.println("Time configured");

      //Create time update task after wifi and time setup
     xTaskCreatePinnedToCore(timeTask, "Time Update Task", 2048, NULL, 2, &timeTaskHandle, 1); // Reduced stack size

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

    touch_init(gfx->width(), gfx->height(), gfx->getRotation());

    xTaskCreatePinnedToCore(wifiTask, "WiFi Task", 4096, NULL, 1, &wifiTaskHandle, 1);
    xTaskCreatePinnedToCore(lvglTask, "LVGL Task", 8192, NULL, 1, &lvglTaskHandle, 1);  // Reduced LVGL task stack

    Serial.println("Setup done");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}