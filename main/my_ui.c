// my_ui.c
#include "my_ui.h"
#include "esp_log.h"
#include "esp_random.h" // 用于生成随机数
#include <stdlib.h>     // 标准库
#include <string.h>     // 用于 memset
#include <stdio.h>      // 用于 snprintf
#include <math.h>       // 用于 isfinite()
#include <limits.h>     // 用于 UINT32_MAX

static const char *TAG = "MY_UI_TREEMAP_SOLID_CELLS"; // 更新 TAG

// --- 配置 ---
#define NUM_CELLS 15            // 单元格数量
#define NUM_SECTORS 6           // 板块数量 (与下拉列表选项对应)
#define UPDATE_INTERVAL_MS 2000 // 更新间隔 (毫秒)
#define CELL_BORDER_WIDTH 1     // 单元格边框宽度
#define CELL_RADIUS 0           // 单元格圆角半径
#define CONTAINER_PADDING 5     // 整体容器的内边距
#define CELL_GAP 3              // 单元格之间的间隙
#define MAX_TICKER_LEN 8        // 股票代码最大长度

// --- 数据接口结构体 ---
typedef struct {
    char ticker_symbol[MAX_TICKER_LEN];
    float value;
} stock_data_t;

// --- UI 元素存储结构体 ---
typedef struct {
    lv_obj_t* cell_obj;
    lv_obj_t* ticker_label;
    lv_obj_t* change_label;
} treemap_ui_cell_t;

// --- 全局/静态变量 ---
static stock_data_t current_stock_data[NUM_CELLS];
static treemap_ui_cell_t treemap_ui_cells[NUM_CELLS];
static lv_timer_t * data_update_timer;
static int current_sector_index = 0; // 当前选择的板块索引

// --- 各板块的股票代码列表 (示例) ---
const char* sector_tickers[NUM_SECTORS][NUM_CELLS] = {
    // Sector 0: Tech Services
    {"AAPL", "MSFT", "GOOGL", "AMZN", "META", "TSLA", "CRM", "ACN", "ORCL", "IBM", "ADP", "NOW", "INTU", "FISV", "UBER"},
    // Sector 1: Electronics
    {"NVDA", "AVGO", "ASML", "TXN", "QCOM", "AMD", "INTC", "MU", "ADI", "LRCX", "AMAT", "KLAC", "CSCO", "STM", "NXPI"},
    // Sector 2: Finance
    {"JPM", "V", "MA", "BAC", "WFC", "MS", "GS", "BLK", "AXP", "SPGI", "C", "SCHW", "PNC", "USB", "CB"},
    // Sector 3: Retail
    {"WMT", "COST", "HD", "TGT", "LOW", "TJX", "DG", "ORLY", "AZO", "ROST", "BBY", "KR", "DLTR", "EBAY", "ETSY"},
    // Sector 4: Health Tech
    {"LLY", "JNJ", "UNH", "MRK", "ABBV", "PFE", "TMO", "DHR", "ABT", "BMY", "AMGN", "GILD", "ISRG", "MDT", "SYK"},
    // Sector 5: Others
    {"XOM", "CVX", "NEE", "DUK", "SO", "LIN", "UPS", "CAT", "DE", "HON", "GE", "RTX", "LMT", "BA", "UNP"}
};

// --- 私有函数声明 ---
static void data_update_task(lv_timer_t * timer);
static void update_treemap_ui(const stock_data_t data_array[]);
static lv_color_t get_color_for_value(float value);
static void create_cell_ui_elements(int index, lv_obj_t* parent_cell);
static void sector_dropdown_event_cb(lv_event_t * e);

/**
 * @brief 初始化模拟 Treemap 界面 (纯色单元格)
 */
void my_ui_heatmap_init(void)
{
    ESP_LOGI(TAG, "Initializing simulated Treemap UI with Solid Cells..."); // 更新日志

    lv_disp_t * disp = lv_disp_get_default();
    if (!disp) { ESP_LOGE(TAG, "Failed to get default display!"); return; }
    lv_coord_t screen_width = lv_disp_get_hor_res(disp);
    lv_coord_t screen_height = lv_disp_get_ver_res(disp);
    ESP_LOGI(TAG, "Detected screen size: %dx%d", screen_width, screen_height);
    if (screen_width <= 0 || screen_height <= 0) { ESP_LOGE(TAG, "Invalid screen dimensions!"); return; }

    lv_obj_t * scr = lv_scr_act();
    lv_obj_clean(scr);

    // --- 创建背景容器 ---
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

    // --- 定义 15 个单元格的布局 ---
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


    // --- 创建 Treemap 单元格 UI 框架并初始化数据结构 ---
    memset(treemap_ui_cells, 0, sizeof(treemap_ui_cells));
    memset(current_stock_data, 0, sizeof(current_stock_data));

    for (int i = 0; i < NUM_CELLS; ++i) {
        lv_coord_t cell_w = lv_area_get_width(&predefined_areas[i]);
        lv_coord_t cell_h = lv_area_get_height(&predefined_areas[i]);

        if (cell_w < 1 || cell_h < 1) { /* ... 跳过无效单元格 ... */ continue; }

        lv_obj_t * cell_container = lv_obj_create(main_container);
        // --- 初始化单元格样式 ---
        lv_obj_remove_style_all(cell_container);
        lv_obj_set_pos(cell_container, predefined_areas[i].x1, predefined_areas[i].y1);
        lv_obj_set_size(cell_container, cell_w, cell_h);
        lv_obj_set_style_radius(cell_container, CELL_RADIUS, 0);
        lv_obj_set_style_bg_opa(cell_container, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cell_container, CELL_BORDER_WIDTH, 0);
        lv_obj_set_style_border_opa(cell_container, LV_OPA_COVER, 0);
        lv_obj_set_layout(cell_container, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(cell_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        treemap_ui_cells[i].cell_obj = cell_container;
        create_cell_ui_elements(i, cell_container);

        snprintf(current_stock_data[i].ticker_symbol, MAX_TICKER_LEN, "---");
        current_stock_data[i].value = 0.0f;
    }

    // --- 使用初始数据首次更新 UI ---
    data_update_task(NULL);

    // --- 创建板块选择下拉列表 ---
    lv_obj_t * dd = lv_dropdown_create(scr);
    // ... (下拉列表设置与之前相同) ...
    lv_dropdown_set_options(dd, "Tech Services\nElectronics\nFinance\nRetail\nHealth Tech\nOthers");
    lv_obj_set_width(dd, 140);
    lv_obj_align(dd, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(dd, sector_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_dropdown_set_selected(dd, current_sector_index);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dd, lv_color_hex(0x606060), 0);
    lv_obj_set_style_border_width(dd, 1, 0);
    lv_obj_set_style_radius(dd, 5, 0);
    lv_obj_set_style_text_color(dd, lv_color_white(), 0);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_left(dd, 8, 0);
    lv_obj_set_style_pad_right(dd, 8, 0);
    lv_obj_set_style_pad_top(dd, 5, 0);
    lv_obj_set_style_pad_bottom(dd, 5, 0);
    lv_obj_t * list = lv_dropdown_get_list(dd);
    if (list) {
        lv_obj_set_style_bg_color(list, lv_color_hex(0x303030), 0);
        lv_obj_set_style_border_color(list, lv_color_hex(0x505050), 0);
        lv_obj_set_style_border_width(list, 1, 0);
        lv_obj_set_style_radius(list, 5, 0);
        lv_obj_set_style_text_color(list, lv_color_white(), 0);
        lv_obj_set_style_text_font(list, &lv_font_montserrat_12, 0);
        lv_obj_set_style_pad_all(list, 5, 0);
        lv_obj_set_style_bg_color(list, lv_palette_main(LV_PALETTE_BLUE), LV_PART_SELECTED | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(list, lv_color_white(), LV_PART_SELECTED | LV_STATE_CHECKED);
    }


    // --- 创建定时器 ---
    if (data_update_timer) {
        lv_timer_del(data_update_timer);
        data_update_timer = NULL;
    }
    data_update_timer = lv_timer_create(data_update_task, UPDATE_INTERVAL_MS, NULL);

    ESP_LOGI(TAG, "Simulated Treemap UI initialized with Solid Cells.");
}

/**
 * @brief 创建单个单元格内部的 UI 元素 (标签)
 */
static void create_cell_ui_elements(int index, lv_obj_t* parent_cell)
{
    // 设置父容器（单元格）的内边距
    lv_obj_set_style_pad_ver(parent_cell, 4, 0);
    lv_obj_set_style_pad_hor(parent_cell, 2, 0);

    // 1. Ticker Label
    lv_obj_t* ticker = lv_label_create(parent_cell);
    lv_label_set_text(ticker, "..."); // Placeholder
    lv_coord_t parent_h = lv_obj_get_height(parent_cell);
    const lv_font_t* ticker_font = &lv_font_montserrat_12;
    if (parent_h > 150) ticker_font = &lv_font_montserrat_20;
    else if (parent_h > 80) ticker_font = &lv_font_montserrat_14;
    else if (parent_h <= 40) ticker_font = &lv_font_montserrat_10;
    lv_obj_set_style_text_font(ticker, ticker_font, 0);
    lv_obj_set_style_text_color(ticker, lv_color_white(), 0); // 初始白色
    lv_label_set_long_mode(ticker, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(ticker, lv_pct(100));
    lv_obj_set_style_text_align(ticker, LV_TEXT_ALIGN_CENTER, 0);

    // 2. Change Label
    lv_obj_t* change = lv_label_create(parent_cell);
    lv_label_set_text(change, " "); // Placeholder
    const lv_font_t* change_font = &lv_font_montserrat_10;
    if (parent_h > 150) change_font = &lv_font_montserrat_16;
    else if (parent_h > 80) change_font = &lv_font_montserrat_12;
    lv_obj_clear_flag(change, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(change, change_font, 0);
    lv_obj_set_style_text_color(change, lv_color_white(), 0); // 初始白色
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
    // ... (代码与上一版本相同, 模拟数据生成) ...
    LV_UNUSED(timer);
    ESP_LOGD(TAG, "Updating stock data for sector index: %d", current_sector_index);
    if (current_sector_index < 0 || current_sector_index >= NUM_SECTORS) {
        ESP_LOGE(TAG, "Invalid sector index: %d. Defaulting to 0.", current_sector_index);
        current_sector_index = 0;
    }
    for (int i = 0; i < NUM_CELLS; ++i) {
        strncpy(current_stock_data[i].ticker_symbol, sector_tickers[current_sector_index][i], MAX_TICKER_LEN - 1);
        current_stock_data[i].ticker_symbol[MAX_TICKER_LEN - 1] = '\0';
        uint32_t r = esp_random();
        float normalized = (float)r / (float)UINT32_MAX;
        float temp_value = normalized * (0.9999f * 2.0f) - 0.9999f;
        if (isfinite(temp_value)) {
            current_stock_data[i].value = temp_value;
        } else {
            current_stock_data[i].value = 0.0f;
        }
    }
    update_treemap_ui(current_stock_data);
    ESP_LOGD(TAG, "Data update complete, UI refresh requested.");
}

/**
 * @brief 根据传入的数据数组更新整个 Treemap 的 UI 显示 (纯色美化)
 */
static void update_treemap_ui(const stock_data_t data_array[])
{
    ESP_LOGD(TAG, "Updating Treemap UI from data (Solid Color)...");
    for (int i = 0; i < NUM_CELLS; ++i) {
        treemap_ui_cell_t *ui_cell = &treemap_ui_cells[i];
        const stock_data_t *data = &data_array[i];

        if (ui_cell->cell_obj && ui_cell->ticker_label && ui_cell->change_label) {
            float value = data->value;
            if (!isfinite(value)) { value = 0.0f; }

            // 1. 更新背景色 (纯色)
            lv_color_t base_color = get_color_for_value(value);
            lv_obj_set_style_bg_color(ui_cell->cell_obj, base_color, 0);
            // --- FIX: 移除渐变色设置 ---
            // lv_obj_set_style_bg_grad_color(ui_cell->cell_obj, grad_color, 0);
            // lv_obj_set_style_bg_grad_dir(ui_cell->cell_obj, LV_GRAD_DIR_VER, 0);

            // 2. 更新边框颜色 (使用比背景色稍暗或稍亮的颜色，增加对比度)
            lv_color_t border_color;
            uint8_t brightness = lv_color_brightness(base_color);
            if (brightness > 128) {
                 border_color = lv_color_darken(base_color, LV_OPA_20); // 亮背景用稍暗边框
            } else {
                 border_color = lv_color_lighten(base_color, LV_OPA_20); // 暗背景用稍亮边框
            }
            lv_obj_set_style_border_color(ui_cell->cell_obj, border_color, 0);

            // 3. 更新 Ticker 标签文本
            lv_label_set_text(ui_cell->ticker_label, data->ticker_symbol);

            // 4. 更新 Change% 标签文本
            char buffer[16];
            float percentage_value = value * 100.0f;
            if (isfinite(percentage_value)) {
                int written = snprintf(buffer, sizeof(buffer), "%+.2f%%", percentage_value);
                if (written >= 0 && written < sizeof(buffer)) {
                    lv_label_set_text(ui_cell->change_label, buffer);
                } else { lv_label_set_text(ui_cell->change_label, "ERR"); }
            } else { lv_label_set_text(ui_cell->change_label, "N/A"); }

            // 5. 根据背景色调整文本颜色
            lv_color_t text_color = (brightness < 128) ? lv_color_white() : lv_color_black(); // 阈值可调整
            lv_obj_set_style_text_color(ui_cell->ticker_label, text_color, 0);
            lv_obj_set_style_text_color(ui_cell->change_label, text_color, 0);

        } else { ESP_LOGW(TAG, "UI elements for cell %d missing.", i); }
    }
     ESP_LOGD(TAG, "Treemap UI update complete.");
}


/**
 * @brief 根据数值（股价变动）获取对应的颜色
 */
static lv_color_t get_color_for_value(float value)
{
    // ... (代码与上一版本相同) ...
    if (!isfinite(value)) { return lv_color_hex(0x303030); }
    float clamped_value = LV_CLAMP(-0.05f, value, 0.05f); // 使用 +/- 5% 范围映射颜色

    if (clamped_value > 0.0005f) { // 正值 (绿色区间)
        lv_color_t dark_green = lv_color_hex(0x005000);
        lv_color_t bright_green = lv_color_hex(0x32CD32);
        uint8_t mix_ratio = (uint8_t)(clamped_value * 20.0f * 255.0f);
        mix_ratio = LV_MIN(mix_ratio, 255);
        return lv_color_mix(bright_green, dark_green, mix_ratio);
    } else if (clamped_value < -0.0005f) { // 负值 (红色区间)
        lv_color_t dark_red = lv_color_hex(0x600000);
        lv_color_t bright_red = lv_color_hex(0xFF4500);
        uint8_t mix_ratio = (uint8_t)(-clamped_value * 20.0f * 255.0f);
         mix_ratio = LV_MIN(mix_ratio, 255);
        return lv_color_mix(bright_red, dark_red, mix_ratio);
    } else { // 接近 0 (灰色)
        return lv_color_hex(0x303030); // 深灰色
    }
}

/**
 * @brief 板块选择下拉列表的事件回调函数
 */
static void sector_dropdown_event_cb(lv_event_t * e)
{
    // ... (代码与上一版本相同) ...
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * dropdown = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        int selected_index = lv_dropdown_get_selected(dropdown);
        if (selected_index != current_sector_index) {
            current_sector_index = selected_index;
            char buf[64];
            lv_dropdown_get_selected_str(dropdown, buf, sizeof(buf));
            ESP_LOGI(TAG, "Sector changed to: %s (index: %d)", buf, current_sector_index);
            data_update_task(NULL);
        }
    }
}

/**
 * @brief （可选）销毁或清理 Treemap 界面资源
 */
/*
void my_ui_heatmap_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Treemap UI...");
    if (data_update_timer) {
        lv_timer_del(data_update_timer);
        data_update_timer = NULL;
    }
    // 删除下拉列表 (如果需要)
    // lv_obj_del(sector_dropdown); // 需要将下拉列表对象设为静态或全局变量

    // ... (删除单元格和容器) ...
    ESP_LOGI(TAG, "Treemap UI deinitialized.");
}
*/
