// my_ui.c
#include "my_ui.h"
#include "esp_log.h"
#include "esp_random.h" // 用于生成随机数
#include <stdlib.h>     // 标准库
#include <string.h>     // 用于 memset
#include <stdio.h>      // 用于 snprintf
#include <math.h>       // <<< 包含 math.h 用于 isfinite()
#include <limits.h>     // <<< 包含 limits.h 用于 UINT32_MAX

static const char *TAG = "MY_UI_TREEMAP_API_V6"; // 更新 TAG

// --- 配置 ---
#define NUM_CELLS 15            // 单元格数量
#define UPDATE_INTERVAL_MS 2000 // 更新间隔 (毫秒)
#define CELL_BORDER_WIDTH 1     // 单元格边框宽度
#define CELL_RADIUS 0           // 单元格圆角半径
#define CONTAINER_PADDING 5     // 整体容器的内边距
#define CELL_GAP 3              // 单元格之间的间隙
#define MAX_TICKER_LEN 8        // 股票代码最大长度 (例如 "AAPL", "MSFT")

// --- 数据接口结构体 ---
typedef struct {
    char ticker_symbol[MAX_TICKER_LEN]; // 股票代码/名称 (例如 "AAPL")
    float value;                        // 当日涨跌幅 (小数形式, e.g., +99.99% 存为 0.9999, -50% 存为 -0.50)
} stock_data_t;

// --- UI 元素存储结构体 ---
typedef struct {
    lv_obj_t* cell_obj;
    lv_obj_t* ticker_label;
    lv_obj_t* change_label;
} treemap_ui_cell_t;

// --- 全局/静态变量 ---
static stock_data_t current_stock_data[NUM_CELLS];       // 存储当前股票数据的数组
static treemap_ui_cell_t treemap_ui_cells[NUM_CELLS];    // 存储 UI 元素指针的数组
static lv_timer_t * data_update_timer;                   // 定时器指针

// --- 私有函数声明 ---
static void data_update_task(lv_timer_t * timer); // 定时更新 *数据* 的任务
static void update_treemap_ui(const stock_data_t data_array[]); // 根据数据更新 *UI* 的函数
static lv_color_t get_color_for_value(float value);
static void create_cell_ui_elements(int index, lv_obj_t* parent_cell); // 创建单个单元格的 UI 元素

/**
 * @brief 初始化模拟 Treemap 界面 (数据接口分离)
 */
void my_ui_heatmap_init(void)
{
    ESP_LOGI(TAG, "Initializing simulated Treemap UI (Limit Random Range)..."); // 更新日志

    lv_disp_t * disp = lv_disp_get_default();
    if (!disp) { ESP_LOGE(TAG, "Failed to get default display!"); return; }
    lv_coord_t screen_width = lv_disp_get_hor_res(disp);
    lv_coord_t screen_height = lv_disp_get_ver_res(disp);
    ESP_LOGI(TAG, "Detected screen size: %dx%d", screen_width, screen_height);
    if (screen_width <= 0 || screen_height <= 0) { ESP_LOGE(TAG, "Invalid screen dimensions!"); return; }

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
    if (cont_w <= 0 || cont_h <= 0) { ESP_LOGE(TAG, "Invalid container content dimensions!"); return; }

    // --- 2. 定义 15 个单元格的布局 (Area: x1, y1, x2, y2) ---
    lv_area_t predefined_areas[NUM_CELLS];
    // ... (布局计算与之前相同) ...
    // Cell 0 (1/4)
    predefined_areas[0].x1 = CELL_GAP; predefined_areas[0].y1 = CELL_GAP;
    predefined_areas[0].x2 = cont_w / 2 - CELL_GAP; predefined_areas[0].y2 = cont_h / 2 - CELL_GAP;
    // Cell 1, 2 (1/8)
    predefined_areas[1].x1 = cont_w / 2 + CELL_GAP; predefined_areas[1].y1 = CELL_GAP;
    predefined_areas[1].x2 = cont_w - CELL_GAP; predefined_areas[1].y2 = cont_h / 4 - CELL_GAP;
    predefined_areas[2].x1 = cont_w / 2 + CELL_GAP; predefined_areas[2].y1 = cont_h / 4 + CELL_GAP;
    predefined_areas[2].x2 = cont_w - CELL_GAP; predefined_areas[2].y2 = cont_h / 2 - CELL_GAP;
    // Cell 3, 4, 5, 6 (1/16)
    predefined_areas[3].x1 = CELL_GAP; predefined_areas[3].y1 = cont_h / 2 + CELL_GAP;
    predefined_areas[3].x2 = cont_w / 4 - CELL_GAP; predefined_areas[3].y2 = cont_h * 3 / 4 - CELL_GAP;
    predefined_areas[4].x1 = cont_w / 4 + CELL_GAP; predefined_areas[4].y1 = cont_h / 2 + CELL_GAP;
    predefined_areas[4].x2 = cont_w / 2 - CELL_GAP; predefined_areas[4].y2 = cont_h * 3 / 4 - CELL_GAP;
    predefined_areas[5].x1 = CELL_GAP; predefined_areas[5].y1 = cont_h * 3 / 4 + CELL_GAP;
    predefined_areas[5].x2 = cont_w / 4 - CELL_GAP; predefined_areas[5].y2 = cont_h - CELL_GAP;
    predefined_areas[6].x1 = cont_w / 4 + CELL_GAP; predefined_areas[6].y1 = cont_h * 3 / 4 + CELL_GAP;
    predefined_areas[6].x2 = cont_w / 2 - CELL_GAP; predefined_areas[6].y2 = cont_h - CELL_GAP;
    // Cell 7-14 (1/32)
    lv_coord_t br_x_start = cont_w / 2; lv_coord_t br_y_start = cont_h / 2;
    lv_coord_t br_w = cont_w / 2; lv_coord_t br_h = cont_h / 2;
    lv_coord_t small_cell_w = br_w / 2; lv_coord_t small_cell_h = br_h / 4;
    int cell_idx = 7;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 2; ++col) {
            if (cell_idx < NUM_CELLS) {
                predefined_areas[cell_idx].x1 = br_x_start + col * small_cell_w + CELL_GAP;
                predefined_areas[cell_idx].y1 = br_y_start + row * small_cell_h + CELL_GAP;
                predefined_areas[cell_idx].x2 = br_x_start + (col + 1) * small_cell_w - CELL_GAP;
                predefined_areas[cell_idx].y2 = br_y_start + (row + 1) * small_cell_h - CELL_GAP;
                cell_idx++;
            }
        }
    }


    // --- 3. 创建 UI 框架并初始化数据结构 ---
    memset(treemap_ui_cells, 0, sizeof(treemap_ui_cells));
    memset(current_stock_data, 0, sizeof(current_stock_data));

    for (int i = 0; i < NUM_CELLS; ++i) {
        lv_coord_t cell_w = lv_area_get_width(&predefined_areas[i]);
        lv_coord_t cell_h = lv_area_get_height(&predefined_areas[i]);

        if (cell_w < 1 || cell_h < 1) {
            ESP_LOGW(TAG, "Skipping cell %d UI creation: invalid dimensions w=%d, h=%d", i, cell_w, cell_h);
            continue;
        }
        ESP_LOGD(TAG, "Creating UI elements for cell %d: size %dx%d", i, cell_w, cell_h);

        lv_obj_t * cell_container = lv_obj_create(main_container);
        // ... (设置单元格样式) ...
        lv_obj_remove_style_all(cell_container);
        lv_obj_set_pos(cell_container, predefined_areas[i].x1, predefined_areas[i].y1);
        lv_obj_set_size(cell_container, cell_w, cell_h);
        lv_obj_set_style_radius(cell_container, CELL_RADIUS, 0);
        lv_obj_set_style_bg_opa(cell_container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell_container, CELL_BORDER_WIDTH, 0);
        lv_obj_set_style_border_color(cell_container, lv_color_black(), 0);
        lv_obj_set_style_border_opa(cell_container, LV_OPA_COVER, 0);
        lv_obj_set_layout(cell_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(cell_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(cell_container, 2, 0);
        lv_obj_set_style_pad_hor(cell_container, 1, 0);
        lv_obj_set_style_bg_color(cell_container, lv_color_hex(0x808080), 0);

        treemap_ui_cells[i].cell_obj = cell_container;
        create_cell_ui_elements(i, cell_container);

        // 初始化数据 (占位符)
        snprintf(current_stock_data[i].ticker_symbol, MAX_TICKER_LEN, "---");
        current_stock_data[i].value = 0.0f;
    }

    // --- 4. 使用初始数据首次更新 UI ---
    update_treemap_ui(current_stock_data);

    // --- 5. 创建定时器，用于周期性更新 *数据* ---
    if (data_update_timer) {
        lv_timer_del(data_update_timer);
        data_update_timer = NULL;
    }
    data_update_timer = lv_timer_create(data_update_task, UPDATE_INTERVAL_MS, NULL);

    ESP_LOGI(TAG, "Simulated Treemap UI initialized. Data API ready.");
}

/**
 * @brief 创建单个单元格内部的 UI 元素 (标签)
 */
static void create_cell_ui_elements(int index, lv_obj_t* parent_cell)
{
    // 1. Ticker Label
    lv_obj_t* ticker = lv_label_create(parent_cell);
    lv_label_set_text(ticker, "..."); // Placeholder
    lv_coord_t parent_h = lv_obj_get_height(parent_cell);
    const lv_font_t* ticker_font = &lv_font_montserrat_12;
    if (parent_h > 150) ticker_font = &lv_font_montserrat_20;
    else if (parent_h > 80) ticker_font = &lv_font_montserrat_14;
    else if (parent_h <= 40) ticker_font = &lv_font_montserrat_10;
    lv_obj_set_style_text_font(ticker, ticker_font, 0);
    lv_obj_set_style_text_color(ticker, lv_color_white(), 0);
    lv_label_set_long_mode(ticker, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ticker, lv_pct(100));
    lv_obj_set_style_text_align(ticker, LV_TEXT_ALIGN_CENTER, 0);

    // 2. Change Label
    lv_obj_t* change = lv_label_create(parent_cell);
    lv_label_set_text(change, " "); // Placeholder
    const lv_font_t* change_font = &lv_font_montserrat_10;
    if (parent_h > 150) change_font = &lv_font_montserrat_16;
    else if (parent_h > 80) change_font = &lv_font_montserrat_12;
    lv_obj_clear_flag(change, LV_OBJ_FLAG_HIDDEN); // 确保标签总是可见的
    lv_obj_set_style_text_font(change, change_font, 0);
    lv_obj_set_style_text_color(change, lv_color_white(), 0);
    lv_obj_set_width(change, lv_pct(100));
    lv_obj_set_style_text_align(change, LV_TEXT_ALIGN_CENTER, 0);

    // Store UI pointers
    treemap_ui_cells[index].ticker_label = ticker;
    treemap_ui_cells[index].change_label = change;
}


/**
 * @brief 定时任务：获取数据（当前为模拟）并请求 UI 更新
 */
static void data_update_task(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    ESP_LOGD(TAG, "Updating stock data...");

    // ====================================================================
    // == 接口：填充 current_stock_data 数组 ==
    // ====================================================================
    // --- 示例：使用随机数据模拟 ---
    const char* sample_tickers[NUM_CELLS] = {
        "AAPL", "MSFT", "GOOGL", "NVDA", "AMZN", "TSLA", "META", "JPM", "V",
        "BAC", "WFC", "COST", "DIS", "XOM", "CVX"
    };
    for (int i = 0; i < NUM_CELLS; ++i) {
        strncpy(current_stock_data[i].ticker_symbol, sample_tickers[i], MAX_TICKER_LEN - 1);
        current_stock_data[i].ticker_symbol[MAX_TICKER_LEN - 1] = '\0';

        // --- FIX: 修改随机数生成方式，确保在 -0.9999 到 +0.9999 之间 ---
        uint32_t r = esp_random();
        // 将 32 位无符号整数映射到 0.0f 到 1.0f 的浮点数
        float normalized = (float)r / (float)UINT32_MAX;
        // 将 0.0f-1.0f 映射到 -0.9999f 到 +0.9999f
        float temp_value = normalized * (0.9999f * 2.0f) - 0.9999f;

        // 检查生成的浮点数是否有效
        if (isfinite(temp_value)) {
            current_stock_data[i].value = temp_value;
             ESP_LOGD(TAG, "Generated valid data for cell %d: value=%.4f", i, current_stock_data[i].value);
        } else {
            ESP_LOGE(TAG, "Generated invalid float for cell %d! Setting to 0.", i);
            current_stock_data[i].value = 0.0f; // 如果生成的值无效，强制设为 0
        }
    }
    // --- 随机数据模拟结束 ---
    // ====================================================================

    update_treemap_ui(current_stock_data); // 请求 UI 更新
    ESP_LOGD(TAG, "Data update complete, UI refresh requested.");
}

/**
 * @brief 根据传入的数据数组更新整个 Treemap 的 UI 显示
 */
static void update_treemap_ui(const stock_data_t data_array[])
{
    ESP_LOGD(TAG, "Updating Treemap UI from data...");
    for (int i = 0; i < NUM_CELLS; ++i) {
        treemap_ui_cell_t *ui_cell = &treemap_ui_cells[i];
        const stock_data_t *data = &data_array[i];

        // 检查 UI 对象是否存在
        if (ui_cell->cell_obj && ui_cell->ticker_label && ui_cell->change_label) {
            // 1. 更新背景色
            float value = data->value;
            if (!isfinite(value)) { // 再次检查从数组读取的值
                ESP_LOGE(TAG, "Invalid float value read from data array for cell %d! Using 0.", i);
                value = 0.0f;
            }
            lv_color_t color = get_color_for_value(value);
            lv_obj_set_style_bg_color(ui_cell->cell_obj, color, 0);

            // 2. 更新 Ticker 标签文本
            lv_label_set_text(ui_cell->ticker_label, data->ticker_symbol);

            // 3. 更新 Change% 标签文本
            char buffer[16];
            float percentage_value = value * 100.0f;

            // 再次检查百分比值
            if (isfinite(percentage_value)) {
                int written = snprintf(buffer, sizeof(buffer), "%+.2f%%", percentage_value);

                if (written >= 0 && written < sizeof(buffer)) {
                    lv_label_set_text(ui_cell->change_label, buffer);
                } else {
                     ESP_LOGW(TAG, "snprintf issue for cell %d (value=%.2f, written=%d)", i, percentage_value, written);
                     lv_label_set_text(ui_cell->change_label, "ERR");
                }
            } else {
                ESP_LOGE(TAG, "Invalid percentage value for cell %d: %.4f", i, value);
                lv_label_set_text(ui_cell->change_label, "N/A");
            }

            // 4. 根据背景色调整文本颜色
            lv_color_t bg_color = lv_obj_get_style_bg_color(ui_cell->cell_obj, 0);
            uint8_t brightness = lv_color_brightness(bg_color);
            lv_color_t text_color = (brightness < 100) ? lv_color_white() : lv_color_black();

            lv_obj_set_style_text_color(ui_cell->ticker_label, text_color, 0);
            lv_obj_set_style_text_color(ui_cell->change_label, text_color, 0);

        } else {
             ESP_LOGW(TAG, "UI elements for cell %d are missing, skipping update.", i);
        }
    }
     ESP_LOGD(TAG, "Treemap UI update complete.");
}


/**
 * @brief 根据数值（股价变动）获取对应的颜色
 */
static lv_color_t get_color_for_value(float value)
{
    if (!isfinite(value)) {
        ESP_LOGW(TAG, "get_color_for_value received invalid float, returning gray.");
        return lv_color_hex(0x303030);
    }

    // 将 -0.9999 到 +0.9999 的范围映射到颜色变化
    // 为了让颜色变化更明显，我们可以只用其中的一部分范围，比如 -0.1 到 +0.1
    float clamped_value = LV_CLAMP(-0.1f, value, 0.1f); // 仍然用 +/- 10% 来决定颜色深浅

    if (clamped_value > 0.0005f) { // 正值 (绿色区间)
        lv_color_t dark_green = lv_color_hex(0x005000);
        lv_color_t bright_green = lv_color_hex(0x32CD32);
        // 将 clamped_value (0 to 0.1) 映射到 mix_ratio (0 to 255)
        uint8_t mix_ratio = (uint8_t)(clamped_value * 10.0f * 255.0f);
        mix_ratio = LV_MIN(mix_ratio, 255);
        return lv_color_mix(bright_green, dark_green, mix_ratio);
    } else if (clamped_value < -0.0005f) { // 负值 (红色区间)
        lv_color_t dark_red = lv_color_hex(0x600000);
        lv_color_t bright_red = lv_color_hex(0xFF4500);
        // 将 clamped_value (-0.1 to 0) 的绝对值映射到 mix_ratio (0 to 255)
        uint8_t mix_ratio = (uint8_t)(-clamped_value * 10.0f * 255.0f);
         mix_ratio = LV_MIN(mix_ratio, 255);
        return lv_color_mix(bright_red, dark_red, mix_ratio);
    } else { // 接近 0 (灰色)
        return lv_color_hex(0x303030);
    }
}

/**
 * @brief （可选）销毁或清理 Treemap 界面资源
 */
/*
void my_ui_heatmap_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Treemap UI (Data API)...");
    if (data_update_timer) {
        lv_timer_del(data_update_timer);
        data_update_timer = NULL;
    }
    for (int i = 0; i < NUM_CELLS; ++i) {
        if (treemap_ui_cells[i].cell_obj) {
            lv_obj_del(treemap_ui_cells[i].cell_obj); // 删除容器会删除子标签
            treemap_ui_cells[i].cell_obj = NULL;
            treemap_ui_cells[i].ticker_label = NULL;
            treemap_ui_cells[i].change_label = NULL;
        }
    }
    // 清空数据数组 (如果需要)
    // memset(current_stock_data, 0, sizeof(current_stock_data));
    ESP_LOGI(TAG, "Treemap UI (Data API) deinitialized.");
}
*/
