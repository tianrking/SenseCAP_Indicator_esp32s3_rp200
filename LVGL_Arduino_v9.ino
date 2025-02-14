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

    // --- Top Section (4 Cities) ---
    lv_obj_t *topContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(topContainer, screenWidth, screenHeight / 2);
    lv_obj_set_layout(topContainer, LV_LAYOUT_GRID);
    lv_obj_set_style_pad_all(topContainer, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(topContainer, 0, LV_PART_MAIN); //Remove the border

    lv_coord_t col_dsc[] = {screenWidth / 2, screenWidth / 2, LV_GRID_TEMPLATE_LAST};
    lv_coord_t row_dsc[] = {screenHeight / 4, screenHeight / 4, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(topContainer, col_dsc, row_dsc);

    const char *cityNames[] = {"Barcelona", "Istanbul", "Tokyo", "New York"};
    long timeOffsets[] = {7200, 10800, 32400, -18000}; // Offsets in seconds

    lv_obj_t *cityLabels[4];
    for (int i = 0; i < 4; i++) {
        cityLabels[i] = lv_label_create(topContainer);
        lv_obj_set_style_text_font(cityLabels[i], &lv_font_montserrat_22, LV_PART_MAIN); // Smaller font
        lv_obj_set_style_text_color(cityLabels[i], lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_label_set_text(cityLabels[i], "00:00:00");
        lv_obj_set_grid_cell(cityLabels[i], LV_GRID_ALIGN_CENTER, i % 2, 1,
                             LV_GRID_ALIGN_CENTER, i / 2, 1);

        lv_obj_t *cityNameLabel = lv_label_create(topContainer);
        lv_obj_set_style_text_font(cityNameLabel, &lv_font_montserrat_18, LV_PART_MAIN); // Smaller font
        lv_obj_set_style_text_color(cityNameLabel, lv_color_hex(0xaaaaaa), LV_PART_MAIN);
        lv_label_set_text(cityNameLabel, cityNames[i]);
          lv_obj_set_grid_cell(cityNameLabel, LV_GRID_ALIGN_CENTER, i % 2, 1,
                             LV_GRID_ALIGN_CENTER, (i / 2)+1, 1); // Place in grid
    }

    // --- Bottom Section (Hong Kong) ---
    lv_obj_t *hongKongLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(hongKongLabel, &lv_font_montserrat_22, LV_PART_MAIN); // Smaller font
    lv_obj_set_style_text_color(hongKongLabel, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_label_set_text(hongKongLabel, "00:00:00");
    lv_obj_align(hongKongLabel, LV_ALIGN_BOTTOM_MID, 0, -50); // Adjust vertical position

    lv_obj_t *hongKongCityNameLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(hongKongCityNameLabel, &lv_font_montserrat_18, LV_PART_MAIN); // Smaller Font
    lv_obj_set_style_text_color(hongKongCityNameLabel, lv_color_hex(0xaaaaaa), LV_PART_MAIN);
    lv_label_set_text(hongKongCityNameLabel, "Hong Kong");
    lv_obj_align_to(hongKongCityNameLabel, hongKongLabel, LV_ALIGN_OUT_TOP_MID, 0, -5); // Fine-tune position

    Serial.println("LVGL UI initialized.");

    uint32_t last_lv_task_time = millis();
    while (1) {
        lv_task_handler();
        if (millis() - last_lv_task_time >= 500) { // Update display less frequently
            last_lv_task_time = millis();

            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) { // Get base time (no offset)

              // Update top section cities
              for (int i = 0; i < 4; i++) {
                // Apply offset to tm struct
                struct tm cityTime = timeinfo;
                cityTime.tm_hour += (timeOffsets[i] / 3600);   // Add hours
                cityTime.tm_min += (timeOffsets[i] % 3600) / 60; // Add minutes
                mktime(&cityTime); // Normalize the time (handles overflow)

                char timeString[9];
                strftime(timeString, sizeof(timeString), "%H:%M:%S", &cityTime);
                lv_label_set_text(cityLabels[i], timeString);
              }

              //Update Hong Kong time
              struct tm hkTime = timeinfo;
              hkTime.tm_hour += 8;
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