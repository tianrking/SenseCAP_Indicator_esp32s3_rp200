#include <Arduino.h>
#include "ui_dev.h"

// 初始化函数
void setup() {
    Serial.begin(115200);
    Serial.println("SenseCap Indicator GFX LVGL_Arduino_v9 example ");

    // 初始化UI
    ui_init();

    // 创建FreeRTOS任务
    xTaskCreate(lvgl_task, "LVGL Task", 4096, NULL, 1, NULL);
}

void loop() {
    // 空的loop，任务在FreeRTOS中运行
    delay(100);
}
