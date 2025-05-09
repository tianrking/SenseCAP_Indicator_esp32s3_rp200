// my_ui.h
#ifndef MY_UI_H
#define MY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h" // 引入 LVGL 库

/**
 * @brief 初始化你的自定义 LVGL 界面
 *
 * 这个函数应该创建你界面上的所有控件。
 */
void my_ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MY_UI_H*/