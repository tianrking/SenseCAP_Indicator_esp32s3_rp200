#ifndef MY_UI_H
#define MY_UI_H

#include "lvgl.h" // LVGL v8.3 main header

// 提示: 请确保在 lv_conf.h 或 menuconfig 中启用了以下 LVGL 组件:
// #define LV_USE_LABEL 1
// (此版本不使用 LVGL 按钮，但如果您的链接问题解决，可以添加)
// 以及所需的字体 (例如 Montserrat 16)。

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化太空入侵者游戏 UI (适配 480x480 屏幕, 触摸控制)。
 *
 * 控制:
 * - 点击屏幕左侧区域: 飞船左移。
 * - 点击屏幕中间区域: 飞船发射子弹。
 * - 点击屏幕右侧区域: 飞船右移。
 * - 游戏结束后点击屏幕: 重新开始。
 *
 * 此函数设置游戏所需的所有 UI 元素。
 * 应在 lv_init() 和显示/输入驱动注册后调用。
 */
void my_ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*MY_UI_H*/
