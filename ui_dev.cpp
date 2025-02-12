// #include "ui_dev.h"
// #include <lvgl.h>
// #include <time.h>  // Include the time library for time functionality

// extern lv_disp_t *disp;  // Ensure this is defined and available

// void createClock() {
//     // Create a label on the active screen for the time
//     lv_obj_t *time_label = lv_label_create(lv_disp_get_scr_act(disp));  // Create a label
//     lv_label_set_text(time_label, "00:00:00");  // Initial time text
//     lv_obj_align(time_label, NULL, LV_ALIGN_CENTER, 0, 0);  // Align it to the center of the screen

//     // Initialize the last update time (for checking every second)
//     static uint32_t last_update = 0;

//     while (1) {
//         lv_task_handler();  // Process LVGL tasks

//         // Update the time every second
//         if (millis() - last_update >= 1000) {
//             last_update = millis();

//             // Get the current time (use RTC or the built-in ESP32 time system)
//             struct tm timeinfo;
//             if (!getLocalTime(&timeinfo)) {
//                 Serial.println("Failed to obtain time");
//                 continue;
//             }

//             int hour = timeinfo.tm_hour;
//             int minute = timeinfo.tm_min;
//             int second = timeinfo.tm_sec;

//             // Format the time string to update the label (HH:MM:SS format)
//             char time_str[9];
//             snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", hour, minute, second);

//             // Update the label text with the current time
//             lv_label_set_text(time_label, time_str);  // Set the time on the label
//         }

//         vTaskDelay(5 / portTICK_PERIOD_MS);  // Delay to prevent overloading the task
//     }
// }
