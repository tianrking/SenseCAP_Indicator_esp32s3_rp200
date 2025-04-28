// my_ui.c
#include "my_ui.h"
#include "esp_log.h"
#include "esp_random.h" // 用于生成随机数
#include <stdlib.h>     // 标准库
#include <string.h>     // 用于 memset
#include <stdio.h>      // 用于 snprintf

static const char *TAG = "MY_UI_TREEMAP_15"; // 更新 TAG

// --- 配置 ---
#define NUM_PREDEFINED_CELLS 15 // <<< 更新单元格数量
#define UPDATE_INTERVAL_MS 2000 // 更新间隔 (毫秒)
#define CELL_BORDER_WIDTH 1 // 给单元格加个边框，区分彼此
#define CELL_RADIUS 0       // Treemap 通常不用圆角
#define CONTAINER_PADDING 5 // 整体容器的内边距
#define CELL_GAP 3          // 单元格之间的间隙

// --- 存储单元格对象和数据的结构 ---
typedef struct {
    lv_obj_t* cell_obj;
    lv_obj_t* ticker_label;
    lv_obj_t* change_label;
    float current_value;
    char ticker_symbol[6]; // 存储自动生成的代码 "S0"..."S14"
} treemap_cell_t;

// <<< 更新数组大小
static treemap_cell_t treemap_cells[NUM_PREDEFINED_CELLS];
static lv_timer_t * update_timer;

// --- 私有函数声明 ---
static void update_treemap_task(lv_timer_t * timer);
static lv_color_t get_color_for_value(float value);
static void create_cell_content(treemap_cell_t* cell_info, lv_obj_t* parent_cell);

/**
 * @brief 初始化模拟 Treemap 界面 (15块布局)
 */
void my_ui_heatmap_init(void)
{
    ESP_LOGI(TAG, "Initializing simulated Treemap UI (15 cells layout)...");

    lv_disp_t * disp = lv_disp_get_default();
    if (!disp) {
        ESP_LOGE(TAG, "Failed to get default display!");
        return;
    }
    lv_coord_t screen_width = lv_disp_get_hor_res(disp);
    lv_coord_t screen_height = lv_disp_get_ver_res(disp);
    ESP_LOGI(TAG, "Detected screen size: %dx%d", screen_width, screen_height);

    if (screen_width <= 0 || screen_height <= 0) {
         ESP_LOGE(TAG, "Screen dimensions are invalid (%d x %d)!", screen_width, screen_height);
         // ... (错误处理) ...
         return;
    }

    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);

    // --- 1. 创建背景容器 ---
    lv_obj_t * main_container = lv_obj_create(scr);
    lv_obj_set_size(main_container, screen_width, screen_height);
    lv_obj_set_style_pad_all(main_container, CONTAINER_PADDING, 0);
    lv_obj_set_style_border_width(main_container, 0, 0);
    lv_obj_set_style_bg_color(main_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, 0);
    lv_obj_align(main_container, LV_ALIGN_CENTER, 0, 0);

    // --- 计算内容区域尺寸 ---
    lv_coord_t cont_w = screen_width - 2 * CONTAINER_PADDING;
    lv_coord_t cont_h = screen_height - 2 * CONTAINER_PADDING;
    ESP_LOGI(TAG, "Calculated container content area size: %dx%d", cont_w, cont_h);

    if (cont_w <= 0 || cont_h <= 0) {
        ESP_LOGE(TAG, "Calculated container content dimensions are invalid (%d x %d)!", cont_w, cont_h);
        // ... (错误处理) ...
        return;
    }

    // --- 2. 定义 15 个单元格的布局 (Area: x1, y1, x2, y2) ---
    // 基于内容区域 cont_w, cont_h
    // 注意整数除法可能带来的误差，尽量保持计算精确
    lv_area_t predefined_areas[NUM_PREDEFINED_CELLS];

    // --- Top-Left (1/4) ---
    // Cell 0
    predefined_areas[0].x1 = CELL_GAP;
    predefined_areas[0].y1 = CELL_GAP;
    predefined_areas[0].x2 = cont_w / 2 - CELL_GAP;
    predefined_areas[0].y2 = cont_h / 2 - CELL_GAP;

    // --- Top-Right (2 x 1/8) ---
    // Cell 1 (Top half of Top-Right)
    predefined_areas[1].x1 = cont_w / 2 + CELL_GAP;
    predefined_areas[1].y1 = CELL_GAP;
    predefined_areas[1].x2 = cont_w - CELL_GAP;
    predefined_areas[1].y2 = cont_h / 4 - CELL_GAP;
    // Cell 2 (Bottom half of Top-Right)
    predefined_areas[2].x1 = cont_w / 2 + CELL_GAP;
    predefined_areas[2].y1 = cont_h / 4 + CELL_GAP;
    predefined_areas[2].x2 = cont_w - CELL_GAP;
    predefined_areas[2].y2 = cont_h / 2 - CELL_GAP;

    // --- Bottom-Left (4 x 1/16) --- (2x2 grid in Bottom-Left quadrant)
    // Cell 3 (Top-Left of BL quadrant)
    predefined_areas[3].x1 = CELL_GAP;
    predefined_areas[3].y1 = cont_h / 2 + CELL_GAP;
    predefined_areas[3].x2 = cont_w / 4 - CELL_GAP;
    predefined_areas[3].y2 = cont_h * 3 / 4 - CELL_GAP;
    // Cell 4 (Top-Right of BL quadrant)
    predefined_areas[4].x1 = cont_w / 4 + CELL_GAP;
    predefined_areas[4].y1 = cont_h / 2 + CELL_GAP;
    predefined_areas[4].x2 = cont_w / 2 - CELL_GAP;
    predefined_areas[4].y2 = cont_h * 3 / 4 - CELL_GAP;
    // Cell 5 (Bottom-Left of BL quadrant)
    predefined_areas[5].x1 = CELL_GAP;
    predefined_areas[5].y1 = cont_h * 3 / 4 + CELL_GAP;
    predefined_areas[5].x2 = cont_w / 4 - CELL_GAP;
    predefined_areas[5].y2 = cont_h - CELL_GAP;
    // Cell 6 (Bottom-Right of BL quadrant)
    predefined_areas[6].x1 = cont_w / 4 + CELL_GAP;
    predefined_areas[6].y1 = cont_h * 3 / 4 + CELL_GAP;
    predefined_areas[6].x2 = cont_w / 2 - CELL_GAP;
    predefined_areas[6].y2 = cont_h - CELL_GAP;

    // --- Bottom-Right (8 x 1/32) --- (2 columns, 4 rows in BR quadrant)
    lv_coord_t br_x_start = cont_w / 2;
    lv_coord_t br_y_start = cont_h / 2;
    lv_coord_t br_w = cont_w / 2; // BR quadrant width
    lv_coord_t br_h = cont_h / 2; // BR quadrant height
    lv_coord_t small_cell_w = br_w / 2; // Width of 1/32 cell
    lv_coord_t small_cell_h = br_h / 4; // Height of 1/32 cell

    int cell_idx = 7; // Start index for the 8 small cells
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 2; ++col) {
            if (cell_idx < NUM_PREDEFINED_CELLS) {
                predefined_areas[cell_idx].x1 = br_x_start + col * small_cell_w + CELL_GAP;
                predefined_areas[cell_idx].y1 = br_y_start + row * small_cell_h + CELL_GAP;
                predefined_areas[cell_idx].x2 = br_x_start + (col + 1) * small_cell_w - CELL_GAP;
                predefined_areas[cell_idx].y2 = br_y_start + (row + 1) * small_cell_h - CELL_GAP;
                cell_idx++;
            }
        }
    }

    // --- 3. 创建预定义的单元格对象 ---
    memset(treemap_cells, 0, sizeof(treemap_cells));

    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        lv_coord_t cell_w = lv_area_get_width(&predefined_areas[i]);
        lv_coord_t cell_h = lv_area_get_height(&predefined_areas[i]);

        if (cell_w < 1 || cell_h < 1) {
            ESP_LOGW(TAG, "Skipping cell %d due to invalid dimensions: w=%d, h=%d", i, cell_w, cell_h);
            continue;
        }
        ESP_LOGD(TAG, "Creating cell %d: (%d, %d) size %dx%d", i, predefined_areas[i].x1, predefined_areas[i].y1, cell_w, cell_h);

        lv_obj_t * cell_container = lv_obj_create(main_container);
        lv_obj_remove_style_all(cell_container);
        lv_obj_set_pos(cell_container, predefined_areas[i].x1, predefined_areas[i].y1);
        lv_obj_set_size(cell_container, cell_w, cell_h);

        // --- 样式设置 (与之前类似) ---
        lv_obj_set_style_radius(cell_container, CELL_RADIUS, 0);
        lv_obj_set_style_bg_opa(cell_container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell_container, CELL_BORDER_WIDTH, 0);
        lv_obj_set_style_border_color(cell_container, lv_color_black(), 0);
        lv_obj_set_style_border_opa(cell_container, LV_OPA_COVER, 0);
        lv_obj_set_layout(cell_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(cell_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(cell_container, 2, 0); // 减少一点垂直 padding
        lv_obj_set_style_pad_hor(cell_container, 1, 0);

        // 初始颜色
        lv_obj_set_style_bg_color(cell_container, lv_color_hex(0x808080), 0);

        // --- 存储信息 ---
        treemap_cells[i].cell_obj = cell_container;
        treemap_cells[i].current_value = 0.0f;
        // 自动生成 Ticker Symbol "S0", "S1", ... "S14"
        snprintf(treemap_cells[i].ticker_symbol, sizeof(treemap_cells[i].ticker_symbol), "S%d", i);

        // 创建单元格内的标签
        create_cell_content(&treemap_cells[i], cell_container);
    }

    // --- 4. 初始化随机数据并首次更新 ---
    update_treemap_task(NULL);

    // --- 5. 创建定时器 ---
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    update_timer = lv_timer_create(update_treemap_task, UPDATE_INTERVAL_MS, NULL);

    ESP_LOGI(TAG, "Simulated Treemap UI (15 cells) initialized successfully.");
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
    lv_label_set_text(ticker, cell_info->ticker_symbol);

    // 根据父容器高度估算合适的字体大小 (调整阈值以适应更小的块)
    lv_coord_t parent_h = lv_obj_get_height(parent_cell);
    const lv_font_t* ticker_font = &lv_font_montserrat_12; // 默认小号
    if (parent_h > 150) { // 最大的块 (1/4)
        ticker_font = &lv_font_montserrat_20;
    } else if (parent_h > 80) { // 中等块 (1/8, 1/16)
        ticker_font = &lv_font_montserrat_14;
    } else if (parent_h <= 40) { // 非常小的块 (1/32 可能小于此)
         ticker_font = &lv_font_montserrat_10; // 最小字体
         // 对于特别小的块，可以考虑只显示 Ticker 或只显示百分比
         // lv_label_set_text(ticker, ""); // 隐藏 Ticker
    }

    lv_obj_set_style_text_font(ticker, ticker_font, 0);
    lv_obj_set_style_text_color(ticker, lv_color_white(), 0);
    lv_label_set_long_mode(ticker, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ticker, lv_pct(100));
    lv_obj_set_style_text_align(ticker, LV_TEXT_ALIGN_CENTER, 0);

    // 2. 创建涨跌幅标签
    lv_obj_t* change = lv_label_create(parent_cell);
    lv_label_set_text(change, "+0.0%");

    const lv_font_t* change_font = &lv_font_montserrat_10; // 默认最小
     if (parent_h > 150) {
        change_font = &lv_font_montserrat_16;
    } else if (parent_h > 80) {
        change_font = &lv_font_montserrat_12;
    }
    // 如果块太小，可能百分比也放不下或看不清
    if (parent_h <= 30) {
        // lv_label_set_text(change, ""); // 隐藏百分比
        lv_obj_add_flag(change, LV_OBJ_FLAG_HIDDEN); // 或者直接隐藏对象
    } else {
        lv_obj_clear_flag(change, LV_OBJ_FLAG_HIDDEN); // 确保可见
    }


    lv_obj_set_style_text_font(change, change_font, 0);
    lv_obj_set_style_text_color(change, lv_color_white(), 0);
    lv_obj_set_width(change, lv_pct(100));
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

    // --- 更新随机数据 ---
    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        if (!treemap_cells[i].cell_obj) continue;
        uint32_t random_val = esp_random();
        treemap_cells[i].current_value = (float)(random_val % 201 - 100) / 100.0f;
    }

    // --- 更新单元格颜色和标签文本 ---
    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        treemap_cell_t *current_cell = &treemap_cells[i];

        if (current_cell->cell_obj) {
            float value = current_cell->current_value;
            lv_color_t color = get_color_for_value(value);
            lv_obj_set_style_bg_color(current_cell->cell_obj, color, 0);

            // 更新涨跌幅标签文本 (如果标签可见)
            if (current_cell->change_label && !lv_obj_has_flag(current_cell->change_label, LV_OBJ_FLAG_HIDDEN)) {
                char buffer[10];
                snprintf(buffer, sizeof(buffer), "%+.1f%%", value * 100.0f);
                lv_label_set_text(current_cell->change_label, buffer);

                // 调整文本颜色
                lv_color_t bg_color = lv_obj_get_style_bg_color(current_cell->cell_obj, 0);
                uint8_t brightness = lv_color_brightness(bg_color);
                lv_color_t text_color = (brightness < 100) ? lv_color_white() : lv_color_black();

                if (current_cell->ticker_label) {
                     lv_obj_set_style_text_color(current_cell->ticker_label, text_color, 0);
                }
                lv_obj_set_style_text_color(current_cell->change_label, text_color, 0);
            }
            // 如果 Ticker 标签也需要根据背景调整颜色（即使百分比隐藏了）
            else if (current_cell->ticker_label) {
                 lv_color_t bg_color = lv_obj_get_style_bg_color(current_cell->cell_obj, 0);
                 uint8_t brightness = lv_color_brightness(bg_color);
                 lv_color_t text_color = (brightness < 100) ? lv_color_white() : lv_color_black();
                 lv_obj_set_style_text_color(current_cell->ticker_label, text_color, 0);
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
    // (颜色映射逻辑保持不变)
    value = LV_CLAMP(-1.0f, value, 1.0f);
    if (value > 0.005f) {
        lv_color_t dark_green = lv_color_hex(0x005000);
        lv_color_t bright_green = lv_color_hex(0x32CD32);
        uint8_t mix_ratio = (uint8_t)(value * 255.0f);
        return lv_color_mix(bright_green, dark_green, mix_ratio);
    } else if (value < -0.005f) {
        lv_color_t dark_red = lv_color_hex(0x600000);
        lv_color_t bright_red = lv_color_hex(0xFF4500);
        uint8_t mix_ratio = (uint8_t)(-value * 255.0f);
        return lv_color_mix(bright_red, dark_red, mix_ratio);
    } else {
        return lv_color_hex(0x303030);
    }
}

/**
 * @brief （可选）销毁或清理 Treemap 界面资源
 */
/*
void my_ui_heatmap_deinit(void)
{
    // (清理逻辑需要更新以遍历 15 个单元格)
    ESP_LOGI(TAG, "Deinitializing Treemap UI (15 cells)...");
    if (update_timer) {
        lv_timer_del(update_timer);
        update_timer = NULL;
    }
    for (int i = 0; i < NUM_PREDEFINED_CELLS; ++i) {
        if (treemap_cells[i].cell_obj) {
            lv_obj_del(treemap_cells[i].cell_obj);
            treemap_cells[i].cell_obj = NULL;
            // ... (其他指针置 NULL)
        }
    }
    // ...
    ESP_LOGI(TAG, "Treemap UI (15 cells) deinitialized.");
}
*/
