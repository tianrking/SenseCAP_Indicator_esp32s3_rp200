// my_ui.h
#ifndef MY_UI_H
#define MY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h" // 引入 LVGL 库

/**
 * @brief 初始化股价热力图界面
 *
 * 创建并显示热力图看板。
 */
void my_ui_heatmap_init(void);

/**
 * @brief （可选）销毁或清理热力图界面资源
 *
 * 如果需要切换界面或释放资源，可以实现此函数。
 */
// void my_ui_heatmap_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MY_UI_H*/
