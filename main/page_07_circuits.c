// page_07_circuits.c
#include "page_07_circuits.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_random.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h> // Needed for cosf, sinf, sqrtf, fabsf

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const char *TAG = "Page7_RingDial_V11_Dir";

// --- Configuration ---
#define NUM_BUTTONS      10
#define BUTTON_SIZE      60
#define RING_RADIUS      170
#define ROTATION_SENSITIVITY 1.5f // Adjust sensitivity of rotation

// --- Static variables ---
static lv_obj_t *ring_container = NULL;
static lv_obj_t *ring_buttons[NUM_BUTTONS];
static lv_style_t style_button_ring;
static float current_ring_angle = 0.0f;
// static float prev_drag_angle = 0.0f; // No longer needed
static bool is_dragging = false;
static lv_point_t ring_center = {0, 0};
static lv_coord_t container_screen_x = 0;
static lv_coord_t container_screen_y = 0;

// --- Color Palette ---
static const lv_color_t button_colors[] = {
    LV_COLOR_MAKE(0xFF, 0xB6, 0xC1), LV_COLOR_MAKE(0xAD, 0xD8, 0xE6), LV_COLOR_MAKE(0x98, 0xFB, 0x98),
    LV_COLOR_MAKE(0xFF, 0xFF, 0xE0), LV_COLOR_MAKE(0xFF, 0xD8, 0xB1), LV_COLOR_MAKE(0xE6, 0xE6, 0xFA),
    LV_COLOR_MAKE(0xAF, 0xEE, 0xEE), LV_COLOR_MAKE(0xFF, 0xE4, 0xE1), LV_COLOR_MAKE(0xBC, 0x8F, 0x8F),
    LV_COLOR_MAKE(0xF0, 0xE6, 0x8C)
};
#define NUM_COLORS (sizeof(button_colors) / sizeof(lv_color_t))

// --- Forward declarations ---
static void button_click_cb(lv_event_t *e);
static lv_obj_t* create_ring_button(lv_obj_t *parent_obj, int index);
static void parent_delete_cb(lv_event_t *e);
static void update_button_positions(void);
static void ring_drag_event_cb(lv_event_t *e);
static void ring_layout_timer_cb(lv_timer_t *timer);
static void reset_dragging_flag_timer_cb(lv_timer_t *timer);


// --- Button Click Callback ---
static void button_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    intptr_t index = (intptr_t)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED && !is_dragging) {
        ESP_LOGI(TAG, "Ring Button index %ld (ID %ld) clicked!", index, index + 1);
    }
}

// --- Helper to create a single ring button ---
static lv_obj_t* create_ring_button(lv_obj_t *parent_obj, int index) {
    lv_obj_t *btn = lv_btn_create(parent_obj);
    if (!btn) return NULL;
    lv_obj_set_size(btn, BUTTON_SIZE, BUTTON_SIZE);
    lv_obj_add_style(btn, &style_button_ring, 0);

    lv_color_t color = button_colors[index % NUM_COLORS];
    lv_obj_set_style_bg_color(btn, color, 0);

    lv_obj_t *label = lv_label_create(btn);
    if (label) {
        char label_text[4]; sprintf(label_text, "%d", index + 1); lv_label_set_text(label, label_text); lv_obj_center(label);
        if (lv_color_brightness(color) < 120) lv_obj_set_style_text_color(label, lv_color_white(), 0);
        else lv_obj_set_style_text_color(label, lv_color_black(), 0);
        lv_obj_set_style_pad_all(label, 0, 0);
    }

    lv_obj_add_event_cb(btn, button_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);
    lv_obj_add_event_cb(btn, ring_drag_event_cb, LV_EVENT_ALL, NULL); // Add drag handler

    return btn;
}

// --- Update Button Positions ---
static void update_button_positions(void) {
    if (ring_center.x == 0 && ring_center.y == 0) return;
    const float angle_step = 2.0f * M_PI / NUM_BUTTONS;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!ring_buttons[i] || !lv_obj_is_valid(ring_buttons[i])) continue;
        float button_angle = current_ring_angle + (float)i * angle_step;
        lv_coord_t x = (lv_coord_t)(ring_center.x + RING_RADIUS * cosf(button_angle) - BUTTON_SIZE / 2);
        lv_coord_t y = (lv_coord_t)(ring_center.y + RING_RADIUS * sinf(button_angle) - BUTTON_SIZE / 2);
        lv_obj_set_pos(ring_buttons[i], x, y);
    }
}

// --- Timer Callback to reset dragging flag ---
static void reset_dragging_flag_timer_cb(lv_timer_t *timer) {
    is_dragging = false;
}

// --- Ring Container & Button Drag Event Callback ---
static void ring_drag_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (ring_center.x == 0 && ring_center.y == 0) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

     // Check if actual dragging occurred to set the flag
     if(lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) {
        is_dragging = true;
    }

    if (code == LV_EVENT_PRESSED) {
        is_dragging = false; // Reset on new press
        // No need to store angle anymore
    } else if (code == LV_EVENT_PRESSING && is_dragging) {
        lv_point_t vect; // Drag vector since last PRESSING event
        lv_indev_get_vect(indev, &vect);

        // Only rotate if there is movement
        if (vect.x != 0 || vect.y != 0) {
            lv_point_t point; // Current point in screen coordinates
            lv_indev_get_point(indev, &point);

            // Calculate point relative to the ring center (screen coordinates)
            lv_coord_t center_screen_x = container_screen_x + ring_center.x;
            lv_coord_t center_screen_y = container_screen_y + ring_center.y;
            float rel_x = (float)point.x - center_screen_x;
            float rel_y = (float)point.y - center_screen_y;

            // Determine rotation direction sign based on drag quadrant and vector
            float sign = 0.0f;
            // Prioritize vertical movement on sides, horizontal on top/bottom
            // Add a dead zone near axes to avoid unstable switching
            float dead_zone_factor = 0.3f; // Ignore drag component if relative position is within 30% of radius near axis
            if (fabsf(rel_x) > fabsf(rel_y) && fabsf(rel_x) > RING_RADIUS * dead_zone_factor) { // Left or Right Side
                 if (rel_x > 0) sign = (vect.y < 0) ? -1.0f : ((vect.y > 0) ? 1.0f : 0.0f); // Right: Up->CCW(-), Down->CW(+)
                 else sign = (vect.y < 0) ? 1.0f : ((vect.y > 0) ? -1.0f : 0.0f);           // Left: Up->CW(+), Down->CCW(-)
            } else if (fabsf(rel_y) > fabsf(rel_x) && fabsf(rel_y) > RING_RADIUS * dead_zone_factor) { // Top or Bottom Side
                 if (rel_y < 0) sign = (vect.x > 0) ? 1.0f : ((vect.x < 0) ? -1.0f : 0.0f); // Top: Right->CW(+), Left->CCW(-)
                 else sign = (vect.x > 0) ? -1.0f : ((vect.x < 0) ? 1.0f : 0.0f);           // Bottom: Right->CCW(-), Left->CW(+)
            }
            // If near diagonals or center, sign might remain 0, resulting in no rotation

            // Calculate rotation amount based on drag magnitude
            float drag_magnitude = sqrtf((float)vect.x * vect.x + (float)vect.y * vect.y);
            // Convert linear drag distance to angular change (angle = distance / radius)
            float angle_increment = drag_magnitude / RING_RADIUS;

            // Update the global ring angle
            current_ring_angle += sign * angle_increment * ROTATION_SENSITIVITY;

            // Update button positions
            update_button_positions();
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // Use a timer to reset the flag shortly after release
        lv_timer_t * timer = lv_timer_create(reset_dragging_flag_timer_cb, 50, NULL);
        if(timer) lv_timer_set_repeat_count(timer, 1);
        else is_dragging = false; // Fallback if timer fails
    }
}


// --- Cleanup callback when parent page is deleted ---
static void parent_delete_cb(lv_event_t *e) {
     lv_event_code_t code = lv_event_get_code(e);
     if (code == LV_EVENT_DELETE) {
         ESP_LOGI(TAG, "Parent object deleted. Cleaning up ring menu.");
         for (int i = 0; i < NUM_BUTTONS; i++) ring_buttons[i] = NULL;
         ring_container = NULL;
     }
}

// --- Timer Callback for Initial Layout ---
static void ring_layout_timer_cb(lv_timer_t *timer) {
    lv_obj_t *parent = (lv_obj_t*)timer->user_data;
    if (!parent || !lv_obj_is_valid(parent) || !ring_container || !lv_obj_is_valid(ring_container)) {
        ESP_LOGE(TAG, "Parent or container invalid in ring_layout_timer_cb!");
        return;
    }
    ESP_LOGI(TAG, "Executing ring_layout_timer_cb...");
    lv_coord_t parent_w = lv_obj_get_content_width(parent);
    lv_coord_t parent_h = lv_obj_get_content_height(parent);
    ring_center.x = parent_w / 2;
    ring_center.y = parent_h / 2; // Center in content area
    lv_area_t cont_coords;
    lv_obj_get_coords(ring_container, &cont_coords);
    container_screen_x = cont_coords.x1;
    container_screen_y = cont_coords.y1;
    ESP_LOGI(TAG, "Parent Content: %dx%d, Center: (%d, %d), Cont Screen: (%d, %d)",
             parent_w, parent_h, ring_center.x, ring_center.y, container_screen_x, container_screen_y);
     if (ring_center.y + RING_RADIUS + BUTTON_SIZE / 2 > parent_h || ring_center.y - RING_RADIUS - BUTTON_SIZE / 2 < 0 ) {
         ESP_LOGW(TAG, "Ring radius %d might be too large for content height %d!", RING_RADIUS, parent_h);
     }
    update_button_positions(); // Set initial positions
    ESP_LOGI(TAG, "Initial button layout complete.");
}


/**
 * @brief Initializes the UI elements for the "Circuits" page (Rotating Ring Menu V11 Directional).
 */
void page_07_circuits_init(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Initializing Page 7: Circuits (Rotating Ring Menu V11 Directional)...");
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE); // Disable scroll

    // Init state
    for(int k=0; k<NUM_BUTTONS; ++k) ring_buttons[k] = NULL;
    current_ring_angle = 0.0f; ring_container = NULL;
    ring_center.x = 0; ring_center.y = 0; container_screen_x = 0; container_screen_y = 0;
    is_dragging = false;

    // Init style
    lv_style_init(&style_button_ring);
    lv_style_set_radius(&style_button_ring, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&style_button_ring, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_bg_opa(&style_button_ring, LV_OPA_COVER);
    lv_style_set_border_width(&style_button_ring, 0);
    lv_style_set_text_color(&style_button_ring, lv_color_black());

    // Create container
    ring_container = lv_obj_create(parent);
    if (!ring_container) { ESP_LOGE(TAG, "Failed to create ring container!"); return; }
    lv_obj_set_size(ring_container, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(ring_container, 0, 0);
    lv_obj_set_style_bg_opa(ring_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring_container, 0, 0);
    lv_obj_set_style_pad_all(ring_container, 0, 0);
    lv_obj_add_event_cb(ring_container, ring_drag_event_cb, LV_EVENT_ALL, NULL); // Add drag handler to container
    lv_obj_add_flag(ring_container, LV_OBJ_FLAG_CLICKABLE);

    // Create Buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
        ring_buttons[i] = create_ring_button(ring_container, i); // This now adds drag handler too
        if (!ring_buttons[i]) { ESP_LOGE(TAG, "Failed to create ring button %d!", i); }
    }

    // Add cleanup handler
    lv_obj_add_event_cb(parent, parent_delete_cb, LV_EVENT_DELETE, NULL);

    // Schedule Initial Layout
    lv_timer_t *layout_timer = lv_timer_create(ring_layout_timer_cb, 50, parent);
    if (layout_timer) {
        lv_timer_set_repeat_count(layout_timer, 1);
        ESP_LOGI(TAG, "Initial layout timer created.");
    } else {
        ESP_LOGE(TAG, "Failed to create layout timer!");
         lv_obj_t * error_label = lv_label_create(parent);
         lv_label_set_text(error_label, "Error:\nLayout timer fail!");
         lv_obj_center(error_label);
    }

    ESP_LOGI(TAG, "Page 7 Initialization Complete (layout scheduled).");
}