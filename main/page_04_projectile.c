// page_04_projectile.c
#include "page_04_projectile.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h" // For PSRAM allocation
#include <math.h>   // For trig functions, sqrtf, etc.
#include <stdio.h>  // For snprintf
#include <stdlib.h> // For abs

static const char *TAG = "page_projectile";

// --- Configuration ---
#define CANVAS_WIDTH        450
#define CANVAS_HEIGHT       250
#define GRAVITY             9.81f // m/s^2
#define TRAJECTORY_POINTS   100   // Number of points to calculate for the trajectory line
#define PIXELS_PER_METER    5.0f  // Scaling factor for drawing (adjust as needed)
#define AXIS_PADDING        25    // Increase padding slightly for labels

// --- Colors ---
#define COLOR_BACKGROUND    lv_color_hex(0x203020) // Dark Greenish background
#define COLOR_AXIS          lv_color_hex(0xAAAAAA) // Grey axes
#define COLOR_TRAJECTORY    lv_color_hex(0xFFBF00) // Amber trajectory

// --- Static variables ---
// UI Elements
static lv_obj_t *canvas = NULL;
static lv_obj_t *label_v0_val = NULL;
static lv_obj_t *label_theta_val = NULL;
static lv_obj_t *label_H_val = NULL; // Max Height
static lv_obj_t *label_R_val = NULL; // Range
static lv_obj_t *label_T_val = NULL; // Time of Flight

// Simulation Parameters
static float current_v0 = 50.0f; // Initial velocity in m/s
static float current_theta_deg = 45.0f; // Launch angle in degrees

// Canvas buffer - Will be allocated dynamically
static lv_color_t *cbuf_psram = NULL;

// Canvas origin (bottom-left after padding)
static lv_point_t canvas_origin = {AXIS_PADDING, CANVAS_HEIGHT - 1 - AXIS_PADDING};

// --- Helper Functions ---
static inline float deg_to_rad(float deg) {
    return deg * (float)M_PI / 180.0f;
}

// Draws the axes on the canvas
static void draw_axes(lv_obj_t *target_canvas) {
    if (!target_canvas) return;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = COLOR_AXIS;
    line_dsc.width = 1;

    // Y-axis (Vertical)
    lv_point_t y_axis[2] = {
        {canvas_origin.x, canvas_origin.y}, // Bottom
        {canvas_origin.x, AXIS_PADDING}     // Top (near top padding)
    };
    lv_canvas_draw_line(target_canvas, y_axis, 2, &line_dsc);

    // X-axis (Horizontal)
    lv_point_t x_axis[2] = {
        {canvas_origin.x, canvas_origin.y}, // Left
        {CANVAS_WIDTH - 1 - AXIS_PADDING, canvas_origin.y} // Right (near right padding)
    };
    lv_canvas_draw_line(target_canvas, x_axis, 2, &line_dsc);

     // Add simple ticks/labels
     lv_draw_label_dsc_t label_dsc;
     lv_draw_label_dsc_init(&label_dsc);
     label_dsc.color = COLOR_AXIS;
     label_dsc.font = &lv_font_montserrat_10; // Use a smaller font
     char tick_buf[10];

     // X-axis ticks and labels
     for (int x_m = 10; ; x_m += 10) { // Start from 10m, increment by 10m
         lv_coord_t tick_x_px = canvas_origin.x + (lv_coord_t)(x_m * PIXELS_PER_METER);
         // Stop if the tick goes beyond the drawable area
         if (tick_x_px > CANVAS_WIDTH - 1 - AXIS_PADDING) break;

         // Draw tick mark
         lv_point_t tick_pts[2] = {{tick_x_px, canvas_origin.y - 3}, {tick_x_px, canvas_origin.y + 3}};
         lv_canvas_draw_line(target_canvas, tick_pts, 2, &line_dsc);

         // Draw label below tick mark
         snprintf(tick_buf, sizeof(tick_buf), "%d", x_m); // Label just the number
         label_dsc.align = LV_TEXT_ALIGN_CENTER; // Center align the number below the tick
         // CORRECTED CALL (removed last arg, set alignment via dsc)
         lv_canvas_draw_text(target_canvas, tick_x_px - 10, canvas_origin.y + 5, 20, &label_dsc, tick_buf);
     }
     // Add X-axis unit label "m" further out
     label_dsc.align = LV_TEXT_ALIGN_LEFT;
     lv_canvas_draw_text(target_canvas, CANVAS_WIDTH - AXIS_PADDING, canvas_origin.y + 5, 20, &label_dsc, "(m)");


     // Y-axis ticks and labels
      for (int y_m = 10; ; y_m += 10) { // Start from 10m, increment by 10m
         lv_coord_t tick_y_px = canvas_origin.y - (lv_coord_t)(y_m * PIXELS_PER_METER);
         // Stop if the tick goes beyond the drawable area
         if (tick_y_px < AXIS_PADDING) break;

         // Draw tick mark
         lv_point_t tick_pts[2] = {{canvas_origin.x - 3, tick_y_px}, {canvas_origin.x + 3, tick_y_px}};
         lv_canvas_draw_line(target_canvas, tick_pts, 2, &line_dsc);

         // Draw label left of the tick mark
         snprintf(tick_buf, sizeof(tick_buf), "%d", y_m); // Label just the number
         label_dsc.align = LV_TEXT_ALIGN_RIGHT; // Right align the number left of the tick
         // CORRECTED CALL (removed last arg, set alignment via dsc)
         lv_canvas_draw_text(target_canvas, canvas_origin.x - 25, tick_y_px - (label_dsc.font->line_height / 2), 20, &label_dsc, tick_buf);
     }
     // Add Y-axis unit label "m" further up
      label_dsc.align = LV_TEXT_ALIGN_CENTER;
      lv_canvas_draw_text(target_canvas, canvas_origin.x - 10, AXIS_PADDING - 15, 20, &label_dsc, "(m)");
}

// Calculates and draws the trajectory
static void draw_trajectory(lv_obj_t *target_canvas, float v0, float theta_deg) {
    if (!target_canvas) return;

    lv_canvas_fill_bg(target_canvas, COLOR_BACKGROUND, LV_OPA_COVER);
    draw_axes(target_canvas);

    if (v0 <= 0.0f || theta_deg < 0.0f || theta_deg > 90.0f) {
        return;
    }
     if (theta_deg == 0.0f || theta_deg == 90.0f) {
         return;
     }

    float theta_rad = deg_to_rad(theta_deg);
    float v0x = v0 * cosf(theta_rad);
    float v0y = v0 * sinf(theta_rad);

    if (fabsf(v0y) < 1e-6) return;

    float time_of_flight = (2.0f * v0y) / GRAVITY;
    if (time_of_flight <= 0) return;

    lv_point_t trajectory_pixels[TRAJECTORY_POINTS];
    int point_count = 0;

    float dt = time_of_flight / (float)(TRAJECTORY_POINTS - 1);
    for (int i = 0; i < TRAJECTORY_POINTS; ++i) {
        float t = i * dt;
        float x_m = v0x * t;
        float y_m = v0y * t - 0.5f * GRAVITY * t * t;

        if (y_m < -0.01f && i > 0) {
            break;
        }

        lv_coord_t px = canvas_origin.x + (lv_coord_t)(x_m * PIXELS_PER_METER);
        lv_coord_t py = canvas_origin.y - (lv_coord_t)(y_m * PIXELS_PER_METER);

        // Only add point if it's reasonably within bounds
        // Allow slight overshoot for line drawing continuity? Maybe not necessary.
        if (px >= 0 && px < CANVAS_WIDTH && py >= 0 && py < CANVAS_HEIGHT) {
             // Avoid adding duplicate points if calculation yields same pixel
            if (point_count == 0 || (trajectory_pixels[point_count-1].x != px || trajectory_pixels[point_count-1].y != py)) {
                trajectory_pixels[point_count].x = px;
                trajectory_pixels[point_count].y = py;
                point_count++;
            }
        } else if (point_count > 0) {
            // If we went out of bounds, stop adding points
            break;
        }
    }

    if (point_count >= 2) {
        lv_draw_line_dsc_t traj_dsc;
        lv_draw_line_dsc_init(&traj_dsc);
        traj_dsc.color = COLOR_TRAJECTORY;
        traj_dsc.width = 2;
        // traj_dsc.round_start = 1; // Optional: round ends
        // traj_dsc.round_end = 1;
        lv_canvas_draw_line(target_canvas, trajectory_pixels, point_count, &traj_dsc);
    } else {
        ESP_LOGW(TAG, "Not enough points (%d) calculated within canvas bounds for trajectory.", point_count);
    }
}

// Updates the result labels (H, R, T)
static void update_result_labels(float v0, float theta_deg) {
    if (v0 < 0) v0 = 0;

    float h_max = 0.0f;
    float range = 0.0f;
    float time_of_flight = 0.0f;

    if (v0 > 0 && theta_deg > 0 && theta_deg <= 90.0f) {
        float theta_rad = deg_to_rad(theta_deg);
        float v0y = v0 * sinf(theta_rad);
        float v0x = v0 * cosf(theta_rad);

        // Ensure GRAVITY is positive before division
        if (GRAVITY > 0) {
             h_max = (v0y * v0y) / (2.0f * GRAVITY);
             time_of_flight = (2.0f * v0y) / GRAVITY;
             range = v0x * time_of_flight;
        }
    }

    if (label_H_val) lv_label_set_text_fmt(label_H_val, "H: %.2f m", h_max);
    if (label_R_val) lv_label_set_text_fmt(label_R_val, "R: %.2f m", range);
    if (label_T_val) lv_label_set_text_fmt(label_T_val, "T: %.2f s", time_of_flight);
}

// --- Event Handlers ---

static void slider_v0_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    current_v0 = (float)lv_slider_get_value(slider);
    if (label_v0_val) lv_label_set_text_fmt(label_v0_val, "%.0f m/s", current_v0);
    update_result_labels(current_v0, current_theta_deg);
}

static void slider_theta_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    current_theta_deg = (float)lv_slider_get_value(slider);
    if (label_theta_val) lv_label_set_text_fmt(label_theta_val, "%.0f°", current_theta_deg);
    update_result_labels(current_v0, current_theta_deg);
}

static void fire_btn_event_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Fire button pressed. v0=%.2f, theta=%.2f", current_v0, current_theta_deg);
    draw_trajectory(canvas, current_v0, current_theta_deg);
}

// --- Cleanup Callback ---
static void projectile_page_delete_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Projectile Page delete event");
    if (cbuf_psram) {
        heap_caps_free(cbuf_psram);
        cbuf_psram = NULL;
        ESP_LOGI(TAG, "Projectile canvas buffer freed.");
    } else {
        ESP_LOGW(TAG, "Canvas buffer pointer was NULL in delete callback");
    }
    canvas = NULL;
    label_v0_val = NULL;
    label_theta_val = NULL;
    label_H_val = NULL;
    label_R_val = NULL;
    label_T_val = NULL;
}

// --- Initialization Function ---

void page_04_projectile_init(lv_obj_t *parent) {
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL!");
        return;
    }
    ESP_LOGI(TAG, "Initializing Projectile Page UI...");

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 5, 0);

    // 1. Allocate Canvas Buffer in PSRAM
    size_t buf_size = CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t);
    cbuf_psram = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (cbuf_psram == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for canvas buffer in PSRAM!", buf_size);
         lv_obj_t *err_label = lv_label_create(parent);
         lv_label_set_text(err_label, "Error: Not enough memory\nfor Projectile Canvas!");
         lv_obj_center(err_label);
        return;
    }
    ESP_LOGI(TAG, "Allocated %d bytes for canvas buffer in PSRAM.", buf_size);

    // 2. Create Canvas
    canvas = lv_canvas_create(parent);
    if (!canvas) {
        ESP_LOGE(TAG, "Failed to create canvas object!");
        heap_caps_free(cbuf_psram);
        cbuf_psram = NULL;
        return;
    }
    lv_canvas_set_buffer(canvas, cbuf_psram, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_obj_add_event_cb(canvas, projectile_page_delete_cb, LV_EVENT_DELETE, NULL); // Add delete cb early
    lv_canvas_fill_bg(canvas, COLOR_BACKGROUND, LV_OPA_COVER);
    draw_axes(canvas);


    // 3. Controls Container
    lv_obj_t *ctrl_cont = lv_obj_create(parent);
    lv_obj_set_width(ctrl_cont, lv_pct(100));
    lv_obj_set_height(ctrl_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ctrl_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ctrl_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ctrl_cont, 8, 0);
    lv_obj_set_style_border_width(ctrl_cont, 0, 0);
    lv_obj_set_style_bg_opa(ctrl_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_top(ctrl_cont, 5, 0);

    // --- v0 Slider Row ---
    lv_obj_t* row_v0 = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_v0, lv_pct(100)); lv_obj_set_height(row_v0, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_v0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_v0, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_v0, 0, 0); lv_obj_set_style_bg_opa(row_v0, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_v0, 0, 0);
    lv_obj_t *label_v0 = lv_label_create(row_v0); lv_label_set_text(label_v0, "Velocity (v0):");
    lv_obj_t *slider_v0 = lv_slider_create(row_v0); lv_obj_set_width(slider_v0, lv_pct(50));
    lv_slider_set_range(slider_v0, 0, 100);
    lv_slider_set_value(slider_v0, (int)(current_v0), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_v0, slider_v0_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_v0_val = lv_label_create(row_v0); lv_obj_set_width(label_v0_val, 70);
    lv_label_set_text_fmt(label_v0_val, "%.0f m/s", current_v0);

    // --- Theta Slider Row ---
    lv_obj_t* row_theta = lv_obj_create(ctrl_cont);
    lv_obj_set_width(row_theta, lv_pct(100)); lv_obj_set_height(row_theta, LV_SIZE_CONTENT); lv_obj_set_flex_flow(row_theta, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_theta, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(row_theta, 0, 0); lv_obj_set_style_bg_opa(row_theta, LV_OPA_TRANSP, 0); lv_obj_set_style_pad_all(row_theta, 0, 0);
    lv_obj_t *label_theta = lv_label_create(row_theta); lv_label_set_text(label_theta, "Angle (θ):");
    lv_obj_t *slider_theta = lv_slider_create(row_theta); lv_obj_set_width(slider_theta, lv_pct(50));
    lv_slider_set_range(slider_theta, 0, 90);
    lv_slider_set_value(slider_theta, (int)(current_theta_deg), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_theta, slider_theta_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    label_theta_val = lv_label_create(row_theta); lv_obj_set_width(label_theta_val, 70);
    lv_label_set_text_fmt(label_theta_val, "%.0f°", current_theta_deg);

    // --- Fire Button ---
    lv_obj_t* fire_btn = lv_btn_create(ctrl_cont);
    lv_obj_set_width(fire_btn, lv_pct(50));
    lv_obj_align(fire_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(fire_btn, fire_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* fire_label = lv_label_create(fire_btn);
    lv_label_set_text(fire_label, "Fire!");
    lv_obj_center(fire_label);


    // 4. Results Display Container
    lv_obj_t *res_cont = lv_obj_create(parent);
    lv_obj_set_width(res_cont, lv_pct(100));
    lv_obj_set_height(res_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(res_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(res_cont, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(res_cont, 5, 0);
    lv_obj_set_style_border_width(res_cont, 0, 0);
    lv_obj_set_style_bg_opa(res_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_top(res_cont, 10, 0);

    label_H_val = lv_label_create(res_cont); lv_label_set_text(label_H_val, "H: 0.00 m");
    label_R_val = lv_label_create(res_cont); lv_label_set_text(label_R_val, "R: 0.00 m");
    label_T_val = lv_label_create(res_cont); lv_label_set_text(label_T_val, "T: 0.00 s");


    // 5. Add cleanup callback to parent
    lv_obj_add_event_cb(parent, projectile_page_delete_cb, LV_EVENT_DELETE, NULL);

    // 6. Initial calculation and display
    update_result_labels(current_v0, current_theta_deg);

    ESP_LOGI(TAG, "Projectile Page UI Initialized.");
}