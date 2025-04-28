// my_ui.c
#include "my_ui.h"
#include "esp_log.h" // 用于日志输出

static const char *TAG = "MY_UI"; // 定义日志标签

// 你可以在这里定义静态变量来存储控件指针，如果需要后续操作的话
// static lv_obj_t * my_label;
// static lv_obj_t * my_button;

/**
 * @brief 初始化你的自定义 LVGL 界面
 */
void my_ui_init(void)
{
    ESP_LOGI(TAG, "Initializing custom UI...");

    // 获取当前的活动屏幕
    lv_obj_t * scr = lv_scr_act();

    // 清除屏幕上的所有对象（可选，如果你想从一个干净的屏幕开始）
    // lv_obj_clean(scr);

    // 创建一个标签控件
    lv_obj_t * label = lv_label_create(scr); // 在当前屏幕上创建标签
    lv_label_set_text(label, "Hello, My Custom UI!"); // 设置标签文本
    lv_obj_center(label); // 将标签居中显示

    // --- 在这里添加你其他的控件创建代码 ---
    // 例如，创建一个按钮：
    /*
    lv_obj_t * btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 100, 50); // 设置按钮大小
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50); // 对齐到中心下方50像素
    lv_obj_add_event_cb(btn, my_button_event_handler, LV_EVENT_CLICKED, NULL); // 添加点击事件回调

    lv_obj_t * btn_label = lv_label_create(btn); // 在按钮上创建标签
    lv_label_set_text(btn_label, "Click Me"); // 设置按钮标签文本
    lv_obj_center(btn_label); // 按钮标签居中
    */

    ESP_LOGI(TAG, "Custom UI initialized successfully.");
}

/*
// 如果需要，定义事件处理函数
static void my_button_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Button Clicked!");
        // 在这里添加按钮点击后的逻辑
        // 例如改变标签文本
        // lv_label_set_text(my_label, "Button was clicked!");
    }
}
*/
