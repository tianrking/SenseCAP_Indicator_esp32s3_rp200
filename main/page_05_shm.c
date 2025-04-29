// page_05_shm.c
#include "page_05_shm.h"
#include "lvgl.h"
#include "esp_log.h"
#include <math.h>   // For sinf, cosf, sqrtf, M_PI
#include <stdio.h>  // For snprintf
#include <stdlib.h> // For abs

static const char *TAG = "page_shm";

// --- Configuration ---
#define ANIM_AREA_WIDTH   450
#define ANIM_AREA_HEIGHT  80
#define CHART_WIDTH       450
#define CHART_HEIGHT      150
#define TIMER_INTERVAL_MS 33 // Simulation update interval (~30 FPS)
#define CHART_POINT_COUNT 100 // Number of points visible on the chart

// --- Default Simulation Parameters ---
#define DEFAULT_MASS        1.0f  // kg
#define DEFAULT_K           5.0f  // N/m
#define DEFAULT_AMPLITUDE   30.0f // Initial displacement (pixels from equilibrium)

// --- Static variables ---
// UI Elements
static lv_obj_t *mass_obj = NULL;
static lv_obj_t *chart = NULL;
static lv_chart_series_t *x_series = NULL;
static lv_chart_series_t *v_series = NULL;
static lv_obj_t *label_m_val = NULL;
static lv_obj_t *label_k_val = NULL;
static lv_obj_t *label_A_val = NULL;
static lv_obj_t *label_time_val = NULL;
static lv_obj_t *label_period_val = NULL;
static lv_obj_t *label_freq_val = NULL;

// Simulation State
static float current_m = DEFAULT_MASS;
static float current_k = DEFAULT_K;
static float current_A = DEFAULT_AMPLITUDE; // Amplitude in pixels for animation
static float sim_time = 0.0f; // Simulation time in seconds
static bool is_running = false;
static lv_timer_t *sim_timer = NULL;

// Animation related
static lv_coord_t equilibrium_x = ANIM_AREA_WIDTH / 2; // Center of animation area

// --- Helper Functions ---

// Calculates physics values and updates labels that depend only on m and k
static void update_physics_labels() {
    if (current_m > 0 && current_k > 0) {
        float omega = sqrtf(current_k / current_m);
        float period = 2.0f * (float)M_PI / omega;
        float freq = 1.0f / period;
        if(label_period_val) lv_label_set_text_fmt(label_period_val, "T: %.2f s", period);
        if(label_freq_val) lv_label_set_text_fmt(label_freq_val, "f: %.2f Hz", freq);
    } else {
         if(label_period_val) lv_label_set_text(label_period_val, "T: --- s");
         if(label_freq_val) lv_label_set_text(label_freq_val, "f: --- Hz");
    }
}


// --- Simulation Update Timer Callback ---
static void update_simulation(lv_timer_t *timer) {
    if (!is_running) {
        return; // Do nothing if paused
    }

    if (current_m <= 0 || current_k <= 0) {
         ESP_LOGW(TAG, "Invalid mass or k value (m=%.2f, k=%.2f)", current_m, current_k);
         return;
    }

    sim_time += (float)TIMER_INTERVAL_MS / 1000.0f; // Increment time

    float omega = sqrtf(current_k / current_m);
    float x_sim = current_A * cosf(omega * sim_time);
    float v_sim = -current_A * omega * sinf(omega * sim_time);

    if (mass_obj) {
        lv_obj_set_x(mass_obj, equilibrium_x + (lv_coord_t)x_sim);
    }

    if (chart && x_series && v_series) {
        lv_chart_set_next_value(chart, x_series, (lv_coord_t)x_sim);
        lv_chart_set_next_value(chart, v_series, (lv_coord_t)v_sim);
    }

    if (label_time_val) {
        lv_label_set_text_fmt(label_time_val, "Time: %.2f s", sim_time);
    }
}

// --- Simulation Control Functions ---

static void reset_simulation() {
    ESP_LOGI(TAG, "Resetting simulation");
    is_running = false;
    if (sim_timer) {
        lv_timer_pause(sim_timer);
    }
    sim_time = 0.0f;

    if (mass_obj) {
        float initial_x_sim = current_A * cosf(0.0f);
        lv_obj_set_x(mass_obj, equilibrium_x + (lv_coord_t)initial_x_sim);
    }

    if (chart && x_series) lv_chart_set_all_value(chart, x_series, 0);
    if (chart && v_series) lv_chart_set_all_value(chart, v_series, 0);
    if (chart) lv_chart_refresh(chart);

    if (label_time_val) lv_label_set_text(label_time_val, "Time: 0.00 s");

    update_physics_labels();
}

static void start_simulation() {
    if (!is_running) {
        ESP_LOGI(TAG, "Starting simulation");
        is_running = true;
        if (!sim_timer) {
            sim_timer = lv_timer_create(update_simulation, TIMER_INTERVAL_MS, NULL);
            if(!sim_timer){
                ESP_LOGE(TAG, "Failed to create simulation timer!");
                is_running = false;
                return;
            }
            ESP_LOGI(TAG, "Simulation timer created.");
        } else {
            lv_timer_resume(sim_timer);
            ESP_LOGI(TAG, "Simulation timer resumed.");
        }
    }
}

static void stop_simulation() {
    if (is_running) {
        ESP_LOGI(TAG, "Stopping simulation");
        is_running = false;
        if (sim_timer) {
            lv_timer_pause(sim_timer);
            ESP_LOGI(TAG, "Simulation timer paused.");
        }
    }
}

// --- Event Handlers ---

static void slider_m_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    current_m = (float)lv_slider_get_value(slider) / 100.0f;
    if (label_m_val) lv_label_set_text_fmt(label_m_val, "%.2f kg", current_m);
    reset_simulation();
}

static void slider_k_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    current_k = (float)lv_slider_get_value(slider) / 10.0f;
    if (label_k_val) lv_label_set_text_fmt(label_k_val, "%.1f N/m", current_k);
    reset_simulation();
}

static void slider_A_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    current_A = (float)lv_slider_get_value(slider);
    if (label_A_val) lv_label_set_text_fmt(label_A_val, "%d px", (int)current_A);
    if(chart){
        float max_v = 1.0f;
        if (current_m > 0 && current_k > 0) {
             float omega = sqrtf(current_k / current_m);
             max_v = fabsf(current_A * omega);
        }
        lv_coord_t y_max = (lv_coord_t)LV_MAX(fabsf(current_A), fabsf(max_v));
        y_max = LV_MAX(y_max, 10);
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -y_max, y_max);
    }
    reset_simulation();
}

static void start_btn_event_cb(lv_event_t *e) {
    start_simulation();
}

static void stop_btn_event_cb(lv_event_t *e) {
    stop_simulation();
}

static void reset_btn_event_cb(lv_event_t *e) {
    reset_simulation();
}

// --- Cleanup Callback ---
static void shm_page_delete_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "SHM Page delete event");
    if (sim_timer) {
        lv_timer_del(sim_timer);
        sim_timer = NULL;
        ESP_LOGI(TAG, "SHM Simulation timer deleted.");
    }
    mass_obj = NULL;
    chart = NULL;
    x_series = NULL;
    v_series = NULL;
    label_m_val = NULL;
    label_k_val = NULL;
    label_A_val = NULL;
    label_time_val = NULL;
    label_period_val = NULL;
    label_freq_val = NULL;
    is_running = false;
    sim_time = 0.0f;
}


// --- Initialization Function ---

void page_05_shm_init(lv_obj_t *parent) {
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL!");
        return;
    }
    ESP_LOGI(TAG, "Initializing SHM Page UI...");

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 5, 0);


    // 1. Animation Area
    lv_obj_t *anim_area = lv_obj_create(parent);
    lv_obj_set_size(anim_area, ANIM_AREA_WIDTH, ANIM_AREA_HEIGHT);
    lv_obj_set_style_border_width(anim_area, 1, 0);
    lv_obj_set_style_border_color(anim_area, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_pad_all(anim_area, 0, 0);
    lv_obj_t* equilibrium_line = lv_line_create(anim_area);
    lv_point_t line_points[] = { {equilibrium_x, 0}, {equilibrium_x, ANIM_AREA_HEIGHT-1} };
    lv_line_set_points(equilibrium_line, line_points, 2);
    lv_obj_set_style_line_dash_width(equilibrium_line, 2, 0);
    lv_obj_set_style_line_dash_gap(equilibrium_line, 2, 0);
    lv_obj_set_style_line_color(equilibrium_line, lv_palette_main(LV_PALETTE_GREY), 0);

    mass_obj = lv_obj_create(anim_area);
    lv_obj_set_size(mass_obj, 30, 30);
    lv_obj_set_style_bg_color(mass_obj, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_radius(mass_obj, 3, 0);
    lv_obj_set_style_border_width(mass_obj, 0, 0);
    lv_obj_align(mass_obj, LV_ALIGN_LEFT_MID, 0, 0);

    // 2. Chart Area
    chart = lv_chart_create(parent);
    lv_obj_set_size(chart, CHART_WIDTH, CHART_HEIGHT);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CHART_POINT_COUNT);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -(int)DEFAULT_AMPLITUDE -10 , (int)DEFAULT_AMPLITUDE+10);

    // *** THIS IS THE CORRECTED LINE ***
    lv_chart_set_div_line_count(chart, 5, 5); // Use singular "line"

    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    x_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    v_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);


    // 3. Controls Area Container
    lv_obj_t *ctrl_cont = lv_obj_create(parent);
    lv_obj_set_width(ctrl_cont, lv_pct(100));
    lv_obj_set_height(ctrl_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctrl_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ctrl_cont, 5, 0);
    lv_obj_set_style_border_width(ctrl_cont, 0, 0);
    lv_obj_set_style_bg_opa(ctrl_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_top(ctrl_cont, 5, 0);

    // --- Mass Slider Row ---
    lv_obj_t* row_m = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_m, lv_pct(100)); lv_obj_set_height(row_m, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_m, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_m, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_m, 0, 0); lv_obj_set_style_bg_opa(row_m, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_m, 0, 0);
    lv_obj_t *label_m = lv_label_create(row_m); lv_label_set_text(label_m, "Mass (m):");
    lv_obj_t *slider_m = lv_slider_create(row_m); lv_obj_set_width(slider_m, lv_pct(55));
    lv_slider_set_range(slider_m, 10, 1000);
    lv_slider_set_value(slider_m, (int)(DEFAULT_MASS * 100.0f), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_m, slider_m_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_m_val = lv_label_create(row_m); lv_obj_set_width(label_m_val, 65);
    lv_label_set_text_fmt(label_m_val, "%.2f kg", DEFAULT_MASS);

    // --- Spring Constant Slider Row ---
    lv_obj_t* row_k = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_k, lv_pct(100)); lv_obj_set_height(row_k, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_k, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_k, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_k, 0, 0); lv_obj_set_style_bg_opa(row_k, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_k, 0, 0);
    lv_obj_t *label_k = lv_label_create(row_k); lv_label_set_text(label_k, "Spring (k):");
    lv_obj_t *slider_k = lv_slider_create(row_k); lv_obj_set_width(slider_k, lv_pct(55));
    lv_slider_set_range(slider_k, 1, 100);
    lv_slider_set_value(slider_k, (int)(DEFAULT_K * 10.0f), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_k, slider_k_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_k_val = lv_label_create(row_k); lv_obj_set_width(label_k_val, 65);
    lv_label_set_text_fmt(label_k_val, "%.1f N/m", DEFAULT_K);

    // --- Amplitude Slider Row ---
    lv_obj_t* row_A = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_A, lv_pct(100)); lv_obj_set_height(row_A, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_A, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_A, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_A, 0, 0); lv_obj_set_style_bg_opa(row_A, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_A, 0, 0);
    lv_obj_t *label_A = lv_label_create(row_A); lv_label_set_text(label_A, "Amplitude:");
    lv_obj_t *slider_A = lv_slider_create(row_A); lv_obj_set_width(slider_A, lv_pct(55));
    lv_coord_t max_A = (ANIM_AREA_WIDTH / 2) - (30/2) - 5;
    lv_slider_set_range(slider_A, 5, max_A);
    lv_slider_set_value(slider_A, (int)DEFAULT_AMPLITUDE, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_A, slider_A_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_A_val = lv_label_create(row_A); lv_obj_set_width(label_A_val, 65);
    lv_label_set_text_fmt(label_A_val, "%d px", (int)DEFAULT_AMPLITUDE);

    // --- Control Buttons Row ---
    lv_obj_t* row_btns = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_btns, lv_pct(100)); lv_obj_set_height(row_btns, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_btns, 0, 0); lv_obj_set_style_bg_opa(row_btns, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_btns, 0, 0); lv_obj_set_style_pad_top(row_btns, 5, 0);
    lv_obj_t *start_btn = lv_btn_create(row_btns); lv_obj_add_event_cb(start_btn, start_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_label = lv_label_create(start_btn); lv_label_set_text(start_label, "Start");
    lv_obj_t *stop_btn = lv_btn_create(row_btns); lv_obj_add_event_cb(stop_btn, stop_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stop_label = lv_label_create(stop_btn); lv_label_set_text(stop_label, "Stop");
    lv_obj_t *reset_btn = lv_btn_create(row_btns); lv_obj_add_event_cb(reset_btn, reset_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *reset_label = lv_label_create(reset_btn); lv_label_set_text(reset_label, "Reset");

    // --- Display Labels Row ---
    lv_obj_t* row_disp = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_disp, lv_pct(100)); lv_obj_set_height(row_disp, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_disp, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_disp, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_disp, 0, 0); lv_obj_set_style_bg_opa(row_disp, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_disp, 0, 0); lv_obj_set_style_pad_top(row_disp, 5, 0);
    label_time_val = lv_label_create(row_disp); lv_label_set_text(label_time_val, "Time: 0.00 s");
    label_period_val = lv_label_create(row_disp); lv_label_set_text(label_period_val, "T: --- s");
    label_freq_val = lv_label_create(row_disp); lv_label_set_text(label_freq_val, "f: --- Hz");

    // 4. Add cleanup callback
    lv_obj_add_event_cb(parent, shm_page_delete_cb, LV_EVENT_DELETE, NULL);

    // 5. Initial state setup
    reset_simulation();

    ESP_LOGI(TAG, "SHM Page UI Initialized.");
}