// page_06_optics.c
#include "page_06_optics.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h" // Required for heap_caps_malloc/free
#include <math.h> // For sin, asin, acos, atan2, M_PI, etc.
#include <stdio.h> // For snprintf
#include <stdlib.h> // For abs

static const char *TAG = "page_optics";

// --- Configuration ---
#define CANVAS_WIDTH  460
#define CANVAS_HEIGHT 250 // Leave space for controls below
#define RAY_LENGTH    100 // Length of rays to draw
#define MAX_N_VALUE   300 // Represents 3.00 for sliders
#define MIN_N_VALUE   100 // Represents 1.00 for sliders

// --- Colors ---
#define COLOR_BACKGROUND lv_color_hex(0x202030) // Dark blueish background
#define COLOR_INTERFACE  lv_color_hex(0xFFFFFF) // White interface line
#define COLOR_NORMAL     lv_color_hex(0x808080) // Grey normal line
#define COLOR_INCIDENT   lv_color_hex(0xFFFF00) // Yellow incident ray
#define COLOR_REFLECTED  lv_color_hex(0xFF8000) // Orange reflected ray
#define COLOR_REFRACTED  lv_color_hex(0x00FFFF) // Cyan refracted ray
#define COLOR_TIR        lv_color_hex(0xFF0000) // Red for TIR indicator

// --- Static variables ---
static lv_obj_t *canvas = NULL; // Keep pointer static within file scope
static lv_obj_t *label_n1_val = NULL;
static lv_obj_t *label_n2_val = NULL;
static lv_obj_t *label_angle_inc_val = NULL;
static lv_obj_t *label_angle_refl_val = NULL;
static lv_obj_t *label_angle_refr_val = NULL;
static lv_obj_t *label_tir = NULL;

// NO static cbuf[] declaration here anymore

static float n1 = 1.00f;
static float n2 = 1.50f;
static float angle_inc_deg = 30.0f;

static lv_point_t incidence_point = { CANVAS_WIDTH / 2, CANVAS_HEIGHT / 2 };

// --- Helper Functions ---
static inline float deg_to_rad(float deg) {
    return deg * (float)M_PI / 180.0f;
}

static inline float rad_to_deg(float rad) {
    return rad * 180.0f / (float)M_PI;
}

// --- Drawing Function ---
// (draw_optics function remains the same as the previous corrected version)
static void draw_optics(void) {
    if (!canvas) return;

    // 1. Clear canvas
    lv_canvas_fill_bg(canvas, COLOR_BACKGROUND, LV_OPA_COVER);

    // 2. Draw interface line
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = COLOR_INTERFACE;
    line_dsc.width = 2;
    lv_point_t interface_points[2] = {
        {0, incidence_point.y},
        {CANVAS_WIDTH - 1, incidence_point.y}
    };
    lv_canvas_draw_line(canvas, interface_points, 2, &line_dsc);

    // 3. Draw normal line (dashed)
    line_dsc.color = COLOR_NORMAL;
    line_dsc.width = 1;
    line_dsc.dash_width = 4;
    line_dsc.dash_gap = 4;
    lv_point_t normal_points[2] = {
        {incidence_point.x, 0},
        {incidence_point.x, CANVAS_HEIGHT - 1}
    };
    lv_canvas_draw_line(canvas, normal_points, 2, &line_dsc);
    lv_draw_line_dsc_init(&line_dsc); // Reset dash for rays

    // 4. Calculate angles in radians
    float angle_inc_rad = deg_to_rad(angle_inc_deg);
    float angle_refl_rad = angle_inc_rad;
    float angle_refl_deg = angle_inc_deg;

    // 5. Calculate refracted angle using Snell's Law
    float sin_theta1 = sinf(angle_inc_rad);
    float sin_theta2_calc = (n1 / n2) * sin_theta1;
    float angle_refr_rad = NAN;
    float angle_refr_deg = NAN;
    bool tir_active = false;

    if (fabsf(sin_theta2_calc) <= 1.0f) {
        angle_refr_rad = asinf(sin_theta2_calc);
        angle_refr_deg = rad_to_deg(angle_refr_rad);
        tir_active = false;
    } else {
        tir_active = true;
    }

    // 6. Draw Incident Ray
    line_dsc.color = COLOR_INCIDENT;
    line_dsc.width = 3;
    lv_point_t incident_points[2];
    incident_points[0] = incidence_point;
    incident_points[1].x = incidence_point.x - (lv_coord_t)(RAY_LENGTH * sinf(angle_inc_rad));
    incident_points[1].y = incidence_point.y - (lv_coord_t)(RAY_LENGTH * cosf(angle_inc_rad));
    lv_canvas_draw_line(canvas, incident_points, 2, &line_dsc);

    // 7. Draw Reflected Ray
    line_dsc.color = COLOR_REFLECTED;
    lv_point_t reflected_points[2];
    reflected_points[0] = incidence_point;
    reflected_points[1].x = incidence_point.x + (lv_coord_t)(RAY_LENGTH * sinf(angle_refl_rad));
    reflected_points[1].y = incidence_point.y - (lv_coord_t)(RAY_LENGTH * cosf(angle_refl_rad));
    lv_canvas_draw_line(canvas, reflected_points, 2, &line_dsc);

    // 8. Draw Refracted Ray (or indicate TIR)
    if (!tir_active) {
        line_dsc.color = COLOR_REFRACTED;
        lv_point_t refracted_points[2];
        refracted_points[0] = incidence_point;
        refracted_points[1].x = incidence_point.x + (lv_coord_t)(RAY_LENGTH * sinf(angle_refr_rad));
        refracted_points[1].y = incidence_point.y + (lv_coord_t)(RAY_LENGTH * cosf(angle_refr_rad));
        lv_canvas_draw_line(canvas, refracted_points, 2, &line_dsc);
        lv_label_set_text(label_tir, "");
    } else {
         lv_label_set_text(label_tir, "TIR!");
         lv_obj_set_style_text_color(label_tir, COLOR_TIR, 0);
    }

    // 9. Update Labels
    char buf[20];
    snprintf(buf, sizeof(buf), "%.1f°", angle_inc_deg);
    lv_label_set_text(label_angle_inc_val, buf);
    snprintf(buf, sizeof(buf), "%.1f°", angle_refl_deg);
    lv_label_set_text(label_angle_refl_val, buf);
    if (!tir_active) {
        snprintf(buf, sizeof(buf), "%.1f°", angle_refr_deg);
        lv_label_set_text(label_angle_refr_val, buf);
    } else {
        lv_label_set_text(label_angle_refr_val, "---");
    }
    snprintf(buf, sizeof(buf), "%.2f", n1);
    lv_label_set_text(label_n1_val, buf);
    snprintf(buf, sizeof(buf), "%.2f", n2);
    lv_label_set_text(label_n2_val, buf);
}


// --- Event Handlers ---
// (slider_n1_event_cb and slider_n2_event_cb remain the same)
static void slider_n1_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    n1 = (float)lv_slider_get_value(slider) / 100.0f;
    draw_optics(); // Redraw with new n1
}

static void slider_n2_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    n2 = (float)lv_slider_get_value(slider) / 100.0f;
    draw_optics(); // Redraw with new n2
}

// (canvas_event_cb remains the same)
static void canvas_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t * indev = lv_indev_get_act();
    if(indev == NULL) return;

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        point.x = LV_CLAMP(0, point.x, CANVAS_WIDTH - 1);
        point.y = LV_CLAMP(0, point.y, CANVAS_HEIGHT - 1);

        lv_coord_t dx = point.x - incidence_point.x;
        lv_coord_t dy = point.y - incidence_point.y;

        if (dy < -1) {
            float angle_rad = atan2f((float)dx, (float)-dy);
            float angle_deg = rad_to_deg(angle_rad);
            if (angle_deg < 0.0f) angle_deg = -angle_deg;
            if (angle_deg > 90.0f) angle_deg = 90.0f;

            if (fabsf(angle_deg - angle_inc_deg) > 0.1f) {
                 angle_inc_deg = angle_deg;
                 draw_optics();
            }
        }
    }
}

// --- NEW: Cleanup Callback ---
static void canvas_delete_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Canvas delete event");
    lv_color_t *buf = (lv_color_t *)lv_event_get_user_data(e);
    if (buf) {
        heap_caps_free(buf); // Free the buffer from PSRAM (or wherever it was allocated)
        ESP_LOGI(TAG, "Canvas buffer freed.");
        // Optionally set canvas pointer back to NULL if it's used elsewhere, although static scope limits this need.
        // canvas = NULL;
    } else {
        ESP_LOGW(TAG, "Canvas buffer pointer was NULL in delete callback");
    }
}


// --- Initialization Function ---

/**
 * @brief Initialize the UI elements for the Optics Simulator (Snell's Law).
 *
 * @param parent The parent LVGL object (typically a tab page).
 */
void page_06_optics_init(lv_obj_t *parent)
{
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL!");
        return;
    }
    ESP_LOGI(TAG, "Initializing Optics Page UI...");

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 5, 0);

    // 1. Calculate buffer size and Allocate Canvas Buffer in PSRAM
    size_t buf_size = CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t);
    lv_color_t *cbuf_psram = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); // Use SPIRAM

    if (cbuf_psram == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for canvas buffer in PSRAM!", buf_size);
        // Optionally try allocating in internal RAM as a fallback, but it will likely fail too based on linker error
        cbuf_psram = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
         if (cbuf_psram == NULL) {
             ESP_LOGE(TAG, "Failed to allocate canvas buffer in Internal RAM either!");
             // Create a label indicating the error on screen?
             lv_obj_t *err_label = lv_label_create(parent);
             lv_label_set_text(err_label, "Error: Not enough memory\nfor Optics Canvas!");
             lv_obj_center(err_label);
             return; // Cannot proceed without buffer
         } else {
             ESP_LOGW(TAG, "Allocated canvas buffer in Internal RAM as PSRAM fallback.");
         }
    } else {
         ESP_LOGI(TAG, "Allocated %d bytes for canvas buffer in PSRAM.", buf_size);
    }


    // 2. Create Canvas
    canvas = lv_canvas_create(parent);
    if (!canvas) {
        ESP_LOGE(TAG, "Failed to create canvas object!");
        heap_caps_free(cbuf_psram); // Free buffer if canvas creation fails
        return;
    }
    lv_canvas_set_buffer(canvas, cbuf_psram, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR); // Use allocated buffer
    lv_obj_add_event_cb(canvas, canvas_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(canvas, COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_COVER, 0);

    // *** ADD DELETE CALLBACK TO FREE BUFFER ***
    lv_obj_add_event_cb(canvas, canvas_delete_cb, LV_EVENT_DELETE, cbuf_psram);


    // 3. Create Controls Container (same as before)
    lv_obj_t *ctrl_cont = lv_obj_create(parent);
    lv_obj_set_width(ctrl_cont, lv_pct(100));
    lv_obj_set_height(ctrl_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctrl_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ctrl_cont, 8, 0);
    lv_obj_set_style_pad_column(ctrl_cont, 10, 0);
    lv_obj_set_style_border_width(ctrl_cont, 0, 0);
    lv_obj_set_style_bg_opa(ctrl_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_top(ctrl_cont, 10, 0);


    // 4. Create Controls (Sliders, Labels - same as before)

    // --- n1 Slider Row ---
    lv_obj_t* row1 = lv_obj_create(ctrl_cont);
    // ... (rest of row1 setup as before) ...
    lv_obj_set_width(row1, lv_pct(100));
    lv_obj_set_height(row1, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row1, 0, 0); lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_t *label_n1 = lv_label_create(row1);
    lv_label_set_text(label_n1, "n1 (Top):");
    lv_obj_t *slider_n1 = lv_slider_create(row1);
    lv_obj_set_width(slider_n1, lv_pct(60));
    lv_slider_set_range(slider_n1, MIN_N_VALUE, MAX_N_VALUE);
    lv_slider_set_value(slider_n1, (int)(n1 * 100.0f), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_n1, slider_n1_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_n1_val = lv_label_create(row1);
    lv_obj_set_width(label_n1_val, 55);
    lv_label_set_text_fmt(label_n1_val, "%.2f", n1);

    // --- n2 Slider Row ---
    lv_obj_t* row2 = lv_obj_create(ctrl_cont);
    // ... (rest of row2 setup as before) ...
    lv_obj_set_width(row2, lv_pct(100));
    lv_obj_set_height(row2, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row2, 0, 0); lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_t *label_n2 = lv_label_create(row2);
    lv_label_set_text(label_n2, "n2 (Bot):");
    lv_obj_t *slider_n2 = lv_slider_create(row2);
    lv_obj_set_width(slider_n2, lv_pct(60));
    lv_slider_set_range(slider_n2, MIN_N_VALUE, MAX_N_VALUE);
    lv_slider_set_value(slider_n2, (int)(n2 * 100.0f), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_n2, slider_n2_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_n2_val = lv_label_create(row2);
    lv_obj_set_width(label_n2_val, 55);
    lv_label_set_text_fmt(label_n2_val, "%.2f", n2);


    // --- Angle Display Row ---
    lv_obj_t* row3 = lv_obj_create(ctrl_cont);
    // ... (rest of row3 setup as before) ...
    lv_obj_set_width(row3, lv_pct(100));
    lv_obj_set_height(row3, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row3, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row3, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row3, 0, 0); lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_top(row3, 10, 0); lv_obj_set_style_pad_all(row3, 0, 0);
    // Incident Col
    lv_obj_t* inc_col = lv_obj_create(row3);
    lv_obj_set_flex_flow(inc_col, LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(inc_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(inc_col, 0, 0); lv_obj_set_style_bg_opa(inc_col, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(inc_col, 0, 0); lv_obj_set_style_pad_row(inc_col, 2, 0);
    lv_obj_t *label_angle_inc = lv_label_create(inc_col); lv_label_set_text(label_angle_inc, "Incident");
    label_angle_inc_val = lv_label_create(inc_col); lv_label_set_text(label_angle_inc_val, "?.?°");
    // Reflected Col
    lv_obj_t* refl_col = lv_obj_create(row3);
    lv_obj_set_flex_flow(refl_col, LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(refl_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(refl_col, 0, 0); lv_obj_set_style_bg_opa(refl_col, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(refl_col, 0, 0); lv_obj_set_style_pad_row(refl_col, 2, 0);
    lv_obj_t *label_angle_refl = lv_label_create(refl_col); lv_label_set_text(label_angle_refl, "Reflected");
    label_angle_refl_val = lv_label_create(refl_col); lv_label_set_text(label_angle_refl_val, "?.?°");
    // Refracted Col
    lv_obj_t* refr_col = lv_obj_create(row3);
    lv_obj_set_flex_flow(refr_col, LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(refr_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(refr_col, 0, 0); lv_obj_set_style_bg_opa(refr_col, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(refr_col, 0, 0); lv_obj_set_style_pad_row(refr_col, 2, 0);
    lv_obj_t *label_angle_refr = lv_label_create(refr_col); lv_label_set_text(label_angle_refr, "Refracted");
    label_angle_refr_val = lv_label_create(refr_col); lv_label_set_text(label_angle_refr_val, "?.?°");
    // TIR Label
    label_tir = lv_label_create(refr_col);
    lv_label_set_text(label_tir, "");
    lv_obj_set_style_text_color(label_tir, COLOR_TIR, 0);
    lv_obj_set_style_text_font(label_tir, &lv_font_montserrat_16, 0);

    // 5. Initial Draw
    draw_optics();

    ESP_LOGI(TAG, "Optics Page UI Initialized.");
}