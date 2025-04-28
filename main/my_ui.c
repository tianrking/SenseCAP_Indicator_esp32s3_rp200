// my_ui.c
#include "my_ui.h"
#include "esp_log.h"
#include "esp_random.h" // 用于生成随机数
#include <stdlib.h>     // 标准库
#include <string.h>     // 用于 memset
#include <stdio.h>      // 用于 snprintf

static const char *TAG = "MY_UI_TREEMAP";

// --- 配置 ---
#define NUM_PREDEFINED_CELLS 9 // 定义几个固定大小的单元格数量
#define UPDATE_INTERVAL_MS 2000 // 更新间隔 (毫秒)
#define CELL_BORDER_WIDTH 1 // 给单元格加个边框，区分彼此
#define CELL_RADIUS 0       // Treemap 通常不用圆角
#define CONTAINER_PADDING 5 // 整体容器的内边距
#define CELL_GAP 3          // 单元格之间的间隙

// --- 存储单元格对象和数据的结构 ---
// 结构体用于存储每个单元格的信息
typedef struct {
    lv_obj_t* cell_obj; // 指向单元格容器对象
    lv_obj_t* ticker_label; // 指向显示代码/名称的标签
    lv_obj_t* change_label; // 指向显示涨跌幅的标签
    float current_value; // 当前模拟数据 (-1.0 to 1.0)
    const char* ticker_symbol; // 股票代码 (预定义)
} treemap_cell_t;

// 使用一维数组存储预定义的单元格
static treemap_cell_t treemap_cells[NUM_PREDEFINED_CELLS];
static lv_timer_t * update_timer;                          // 定时器指针

// --- 私有函数声明 ---
static void update_treemap_task(lv_timer_t * timer);
static lv_color_t get_color_for_value(float value);
static void create_cell_content(treemap_cell_t* cell_info, lv_obj_t* parent_cell); // 修改以接收结构体指针

/**
 * @brief 初始化模拟 Treemap 界面
 */
void my_ui_heatmap_init(void) // 函数名保持不变，方便 app_main 调用
{
    ESP_LOGI(TAG, "Initializing simulated Treemap UI (Layout Fixed v2)..."); // 更新日志信息

    lv_disp_t * disp = lv_disp_get_default();
    if (!disp) {
        ESP_LOGE(TAG, "Failed to get default display!");
        return;
    }
    lv_coord_t screen_width = lv_disp_get_hor_res(disp);
    lv_coord_t screen_height = lv_disp_get_ver_res(disp);
    ESP_LOGI(TAG, "Detected screen size: %dx%d", screen_width, screen_height);

    // 检查屏幕尺寸是否有效
    if (screen_width <= 0 || screen_height <= 0) {
         ESP_LOGE(TAG, "Screen dimensions are invalid (%d x %d)! Check display initialization.", screen_width, screen_height);
         lv_obj_t * err_label = lv_label_create(lv_scr_act());
         lv_label_set_text(err_label, "Error: Screen Init Failed\nCheck Logs");
         lv_obj_center(err_label);
         return;
    }

    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr); // 清理屏幕

    // --- 1. 创建背景容器 ---
    lv_obj_t * main_container = lv_obj_create(scr);
    lv_obj_set_size(main_container, screen_width, screen_height); // 占满屏幕
    lv_obj_set_style_pad_all(main_container, CONTAINER_PADDING, 0); // 设置内边距
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), 0); // 设置黑色背景
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, 0);
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 0); // 确保容器居中

    // --- FIX: 直接计算预期的内容区域尺寸 ---
    // 内容区域宽度 = 屏幕宽度 - 左右 padding
    lv_coord_t cont_w = screen_width - 2 * CONTAINER_PADDING;
    // 内容区域高度 = 屏幕高度 - 上下 padding
    lv_coord_t cont_h = screen_height - 2 * CONTAINER_PADDING;
    ESP_LOGI(TAG, "Calculated container content area size: %dx%d", cont_w, cont_h);

    // 检查计算出的内容区域尺寸是否有效
    if (cont_w <= 0 || cont_h <= 0) {
        ESP_LOGE(TAG, "Calculated container content dimensions are invalid (%d x %d)! Check padding/screen size.", cont_w, cont_h);
        lv_obj_t * err_label = lv_label_create(main_container); // 在容器内显示错误
        lv_label_set_text_fmt(err_label, "Error:\nCont. Size Invalid\n%dx%d", cont_w, cont_h);
        lv_obj_center(err_label);
        return;
    }

    // --- 2. 定义预定义单元格的布局 (Area: x1, y1, x2, y2) ---
    // 坐标相对于容器的内容区域 (0,0)
    // 使用直接计算出的 cont_w 和 cont_h
    lv_area_t predefined_areas[NUM_PREDEFINED_CELLS] = {
        // Cell 0 (Large Top-Left)
        {CELL_GAP, CELL_GAP, (cont_w / 2) - CELL_GAP, (cont_h / 2) - CELL_GAP},
        // Cell 1 (Medium Top-Right 1)
        {(cont_w / 2) + CELL_GAP, CELL_GAP, cont_w - CELL_GAP, (cont_h / 4) - CELL_GAP},
        // Cell 2 (Medium Top-Right 2)
        {(cont_w / 2) + CELL_GAP, (cont_h / 4) + CELL_GAP, cont_w - CELL_GAP, (cont_h / 2) - CELL_GAP},
        // Cell 3 (Medium Bottom-Left 1)
        {CELL_GAP, (cont_h / 2) + CELL_GAP, (cont_w / 4) - CELL_GAP, cont_h - CELL_GAP},
        // Cell 4 (Medium Bottom-Left 2)
        {(cont_w / 4) + CELL_GAP, (cont_h / 2) + CELL_GAP, (cont_w / 2) - CELL_GAP, cont_h - CELL_GAP},
        // Cell 5 (Small Bottom-Right 1)
        {(cont_w / 2) + CELL_GAP, (cont_h / 2) + CELL_GAP, (cont_w * 3 / 4) - CELL_GAP, (cont_h * 3 / 4) - CELL_GAP},
        // Cell 6 (Small Bottom-Right 2)
        {(cont_w * 3 / 4) + CELL_GAP, (cont_h / 2) + CELL_GAP, cont_w - CELL_GAP, (cont_h * 3 / 4) - CELL_GAP},
        // Cell 7 (Small Bottom-Right 3)
        {(cont_w / 2) + CELL_GAP, (cont_h * 3 / 4) + CELL_GAP, (cont_w * 3 / 4) - CELL_GAP, cont_h - CELL_GAP},
        // Cell 8 (Small Bottom-Right 4)
        {(cont_w * 3 / 4) + CELL_GAP, (cont_h * 3 / 4) + CELL_GAP, cont_w - CELL_GAP, cont_h - CELL_GAP},
    };

    // 预定义股票代码 (示例)
    const char* predefined_tickers[NUM_PREDEFINED_CELLS] = {
        "AAPL", "MSFT", "GOOGL", "NVDA", "AMZN", "TSLA", "META", "JPM", "V"
    };


    // --- 3. 创建预定义的单元格对象 ---
    memset(treemap_cells, 0, sizeof(treemap_cells)); // 清零结构体数组

    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        // 检查计算出的区域是否有效 (width > 0 and height > 0)
        // 注意：lv_area_get_width/height 返回的是 coord_t，可能需要检查 >= 1
        lv_coord_t cell_w = lv_area_get_width(&predefined_areas[i]);
        lv_coord_t cell_h = lv_area_get_height(&predefined_areas[i]);

        // 确保宽度和高度至少为 1
        if (cell_w < 1 || cell_h < 1) {
            ESP_LOGW(TAG, "Skipping cell %d due to invalid dimensions: (%d, %d, %d, %d) -> w=%d, h=%d",
                     i, predefined_areas[i].x1, predefined_areas[i].y1, predefined_areas[i].x2, predefined_areas[i].y2, cell_w, cell_h);
            continue; // 跳过无效的单元格
        }
         ESP_LOGD(TAG, "Creating cell %d: (%d, %d, %d, %d) -> w=%d, h=%d",
                     i, predefined_areas[i].x1, predefined_areas[i].y1, predefined_areas[i].x2, predefined_areas[i].y2, cell_w, cell_h);


        // 创建单元格容器对象
        // 注意：对象的父对象是 main_container
        lv_obj_t * cell_container = lv_obj_create(main_container);
        lv_obj_remove_style_all(cell_container); // 移除默认样式

        // 应用预定义的布局 (坐标是相对于父容器的内容区域)
        lv_obj_set_pos(cell_container, predefined_areas[i].x1, predefined_areas[i].y1);
        lv_obj_set_size(cell_container, cell_w, cell_h);

        // 设置样式
        lv_obj_set_style_radius(cell_container, CELL_RADIUS, 0); // 直角
        lv_obj_set_style_bg_opa(cell_container, LV_OPA_COVER, 0); // 背景不透明
        lv_obj_set_style_border_width(cell_container, CELL_BORDER_WIDTH, 0); // 边框
        lv_obj_set_style_border_color(cell_container, lv_color_black(), 0); // 黑色边框
        lv_obj_set_style_border_opa(cell_container, LV_OPA_COVER, 0);

        // 设置内部布局 (垂直排列标签)
        lv_obj_set_layout(cell_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(cell_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER); // 水平、垂直居中
        lv_obj_set_style_pad_ver(cell_container, 3, 0); // 内部垂直 padding
        lv_obj_set_style_pad_hor(cell_container, 1, 0); // 内部水平 padding (小一点)


        // 初始颜色 (灰色)
        lv_obj_set_style_bg_color(cell_container, lv_color_hex(0x808080), 0);

        // 存储信息
        treemap_cells[i].cell_obj = cell_container;
        treemap_cells[i].current_value = 0.0f;
        treemap_cells[i].ticker_symbol = predefined_tickers[i]; // 设置股票代码

        // 创建单元格内的标签
        create_cell_content(&treemap_cells[i], cell_container); // 传递结构体指针
    }

    // --- 4. 初始化随机数据并首次更新 ---
    update_treemap_task(NULL); // 首次调用以填充初始颜色和文本

    // --- 5. 创建定时器 ---
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    update_timer = lv_timer_create(update_treemap_task, UPDATE_INTERVAL_MS, NULL);

    ESP_LOGI(TAG, "Simulated Treemap UI initialized successfully.");
}

/**
 * @brief 创建单元格内的标签
 * @param cell_info 指向包含单元格信息的结构体
 * @param parent_cell 父单元格容器对象
 */
static void create_cell_content(treemap_cell_t* cell_info, lv_obj_t* parent_cell)
{
    // 1. 创建股票代码标签
    lv_obj_t* ticker = lv_label_create(parent_cell);
    lv_label_set_text(ticker, cell_info->ticker_symbol); // 使用预定义的代码

    // 根据父容器高度估算合适的字体大小
    lv_coord_t parent_h = lv_obj_get_height(parent_cell);
    const lv_font_t* ticker_font = &lv_font_montserrat_14; // 默认字体
    if (parent_h > 120) { // 调整阈值
        ticker_font = &lv_font_montserrat_20;
    } else if (parent_h < 70) { // 调整阈值
        ticker_font = &lv_font_montserrat_12;
    } else if (parent_h < 50) { // 更小块
         ticker_font = &lv_font_montserrat_10;
    }

    lv_obj_set_style_text_font(ticker, ticker_font, 0);
    lv_obj_set_style_text_color(ticker, lv_color_white(), 0); // 初始白色，后面会根据背景调整
    lv_label_set_long_mode(ticker, LV_LABEL_LONG_CLIP); // 直接裁剪
    lv_obj_set_width(ticker, lv_pct(100)); // 宽度占满父容器 (Flex 会处理)
    lv_obj_set_style_text_align(ticker, LV_TEXT_ALIGN_CENTER, 0);

    // 2. 创建涨跌幅标签
    lv_obj_t* change = lv_label_create(parent_cell);
    lv_label_set_text(change, "+0.0%"); // 初始文本

    const lv_font_t* change_font = &lv_font_montserrat_12; // 默认
     if (parent_h > 120) { // 调整阈值
        change_font = &lv_font_montserrat_16;
    } else if (parent_h < 70) { // 调整阈值
        change_font = &lv_font_montserrat_10;
    } else if (parent_h < 50) { // 更小块
         change_font = &lv_font_montserrat_10; // 最小用10
    }
    lv_obj_set_style_text_font(change, change_font, 0);
    lv_obj_set_style_text_color(change, lv_color_white(), 0); // 初始白色
    lv_obj_set_width(change, lv_pct(100)); // 宽度占满
    lv_obj_set_style_text_align(change, LV_TEXT_ALIGN_CENTER, 0);

    // 存储标签指针
    cell_info->ticker_label = ticker;
    cell_info->change_label = change;
}


/**
 * @brief 定时任务：更新 Treemap 数据、颜色和标签文本
 * @param timer 定时器对象指针
 */
static void update_treemap_task(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    // --- 接口：未来从这里获取真实股价数据 ---
    // 现在，我们为每个预定义的单元格生成随机数据
    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        // 跳过未成功创建的单元格 (cell_obj 为 NULL)
        if (!treemap_cells[i].cell_obj) continue;

        uint32_t random_val = esp_random();
        treemap_cells[i].current_value = (float)(random_val % 201 - 100) / 100.0f;
    }
    // --- 数据获取结束 ---


    // --- 更新单元格颜色和标签文本 ---
    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        treemap_cell_t *current_cell = &treemap_cells[i];

        // 再次检查对象是否存在
        if (current_cell->cell_obj) {
            float value = current_cell->current_value;
            lv_color_t color = get_color_for_value(value);

            // 更新背景色
            lv_obj_set_style_bg_color(current_cell->cell_obj, color, 0);

            // 更新涨跌幅标签文本
            if (current_cell->change_label) {
                char buffer[10];
                snprintf(buffer, sizeof(buffer), "%+.1f%%", value * 100.0f);
                lv_label_set_text(current_cell->change_label, buffer);

                // 根据背景色亮度调整文本颜色，确保可读性
                lv_color_t bg_color = lv_obj_get_style_bg_color(current_cell->cell_obj, 0);
                uint8_t brightness = lv_color_brightness(bg_color);
                // 调整亮度阈值，深色背景用白色字，浅色背景用黑色字
                lv_color_t text_color = (brightness < 100) ? lv_color_white() : lv_color_black(); // 阈值可以调整

                if (current_cell->ticker_label) {
                     lv_obj_set_style_text_color(current_cell->ticker_label, text_color, 0);
                }
                lv_obj_set_style_text_color(current_cell->change_label, text_color, 0);
            }
        }
    }
    // ESP_LOGD(TAG, "Treemap update complete.");
}

/**
 * @brief 根据数值（股价变动）获取对应的颜色
 * @param value 股价变动值，范围通常在 -1.0 到 1.0 之间
 * @return 对应的 LVGL 颜色
 */
static lv_color_t get_color_for_value(float value)
{
    // 限制 value 在 -1.0 到 1.0 之间
    value = LV_CLAMP(-1.0f, value, 1.0f);

    // 调整颜色映射，使其更接近 TradingView 的深红和深绿
    if (value > 0.005f) { // 正值 (绿色区间)
        // 从深绿到亮绿过渡
        lv_color_t dark_green = lv_color_hex(0x005000); // 更深的绿
        lv_color_t bright_green = lv_color_hex(0x32CD32); // 亮绿
        // 使用 mix 时，比例是第二个颜色所占的比例
        uint8_t mix_ratio = (uint8_t)(value * 255.0f); // value=1 -> mix=255 (全为 bright_green)
        // mix(目标色, 基准色, 比例) -> 比例是目标色的混合度
        return lv_color_mix(bright_green, dark_green, mix_ratio); // 混合亮绿和深绿

    } else if (value < -0.005f) { // 负值 (红色区间)
        lv_color_t dark_red = lv_color_hex(0x600000); // 更深的红
        lv_color_t bright_red = lv_color_hex(0xFF4500); // 橙红/亮红
        uint8_t mix_ratio = (uint8_t)(-value * 255.0f); // -value=1 -> mix=255 (全为 bright_red)
        return lv_color_mix(bright_red, dark_red, mix_ratio); // 混合亮红和深红

    } else { // 接近 0 (灰色)
        return lv_color_hex(0x303030); // 使用更深的灰色作为中间色
    }
}

/**
 * @brief （可选）销毁或清理 Treemap 界面资源
 */
/*
void my_ui_heatmap_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Treemap UI...");
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    // 需要手动删除创建的对象
    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        if (treemap_cells[i].cell_obj) {
            lv_obj_del(treemap_cells[i].cell_obj); // 删除单元格会同时删除其子对象(标签)
            treemap_cells[i].cell_obj = NULL;
            treemap_cells[i].ticker_label = NULL;
            treemap_cells[i].change_label = NULL;
        }
    }
    // 如果创建了 main_container，也需要删除
    // lv_obj_del(main_container); // 如果 main_container 是静态或全局的

    ESP_LOGI(TAG, "Treemap UI deinitialized.");
}
*/
