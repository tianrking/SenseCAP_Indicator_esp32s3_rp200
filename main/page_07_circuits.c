// page_07_circuits.c
#include "page_07_circuits.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_random.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const char *TAG = "Page7_RingDial_V14_NoPressFX"; // Updated Tag

// --- Configuration ---
#define NUM_BUTTONS         10
#define BUTTON_SIZE         60
#define RING_RADIUS         170
#define ROTATION_SENSITIVITY 1.5f
#define INERTIA_DECEL_FACTOR 0.92f
#define INERTIA_STOP_THRESHOLD 0.002f
#define INERTIA_TIMER_PERIOD 20
// #define CLICK_ANIM_SCALE    288 // No longer needed
// #define CLICK_ANIM_MS       150 // No longer needed
// #define PRESS_SCALE_ZOOM    295 // No longer needed
// #define PRESS_TRANSITION_MS 100 // No longer needed


// --- Static variables ---
static lv_obj_t *ring_container = NULL;
static lv_obj_t *ring_buttons[NUM_BUTTONS];
static lv_style_t style_button_ring;        // Default button style ONLY
// static lv_style_t style_button_ring_pressed; // <<< REMOVED
static lv_style_t style_page_bg;            // Page background style
static float current_ring_angle = 0.0f;
static bool is_dragging = false;
static lv_point_t ring_center = {0, 0};
static lv_coord_t container_screen_x = 0;
static lv_coord_t container_screen_y = 0;
static lv_timer_t *inertia_timer = NULL;
static float inertia_velocity = 0.0f;

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
static void init_styles(void);
static void inertia_timer_cb(lv_timer_t *timer);
// Removed callbacks related to previous animations/timers
// static void reset_dragging_flag_timer_cb(lv_timer_t *timer);
// static void set_zoom_anim_cb(void * var, int32_t v);
// static void scale_anim_ready_cb(lv_anim_t *a);


// --- Button Click Callback ---
static void button_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    intptr_t index = (intptr_t)lv_event_get_user_data(e);
    // Still check is_dragging to prevent clicks during/after drag
    if (code == LV_EVENT_CLICKED && !is_dragging) {
        ESP_LOGI(TAG, "Ring Button index %ld (ID %ld) clicked!", index, index + 1);
        // No visual animation here anymore
    }
}

// --- Helper to create a single ring button ---
static lv_obj_t* create_ring_button(lv_obj_t *parent_obj, int index) {
    lv_obj_t *btn = lv_btn_create(parent_obj);
    if (!btn) return NULL;
    lv_obj_set_size(btn, BUTTON_SIZE, BUTTON_SIZE);

    // Add base style ONLY
    lv_obj_add_style(btn, &style_button_ring, 0);
    // --- REMOVED PRESSED STYLE ---

    // Set the specific background color
    lv_color_t color = button_colors[index % NUM_COLORS];
    lv_obj_set_style_bg_color(btn, color, 0);

    // Add label
    lv_obj_t *label = lv_label_create(btn);
    if (label) {
        char label_text[4]; sprintf(label_text, "%d", index + 1); lv_label_set_text(label, label_text); lv_obj_center(label);
        if (lv_color_brightness(color) < 120) lv_obj_set_style_text_color(label, lv_color_white(), 0);
        else lv_obj_set_style_text_color(label, lv_color_black(), 0);
        lv_obj_set_style_pad_all(label, 0, 0);
    }

    // Add handlers
    lv_obj_add_event_cb(btn, button_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)index);
    lv_obj_add_event_cb(btn, ring_drag_event_cb, LV_EVENT_ALL, (void*)(intptr_t)index);

    return btn;
}

// --- Update Button Positions ---
static void update_button_positions(void) {
    // ... (Same as previous versions) ...
     if (ring_center.x == 0 && ring_center.y == 0) return;
    const float angle_step = 2.0f * M_PI / NUM_BUTTONS;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!ring_buttons[i] || !lv_obj_is_valid(ring_buttons[i])) continue;
        float button_angle = fmodf(current_ring_angle + (float)i * angle_step, 2.0f * M_PI);
        lv_coord_t x = (lv_coord_t)(ring_center.x + RING_RADIUS * cosf(button_angle) - BUTTON_SIZE / 2);
        lv_coord_t y = (lv_coord_t)(ring_center.y + RING_RADIUS * sinf(button_angle) - BUTTON_SIZE / 2);
        lv_obj_set_pos(ring_buttons[i], x, y);
    }
}

// --- Ring Button Drag Event Callback ---
static void ring_drag_event_cb(lv_event_t *e) {
    // ... (Same drag logic as V12 for rotation, including inertia velocity update) ...
     lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target_btn = lv_event_get_target(e);
    if (ring_center.x == 0 && ring_center.y == 0 && code != LV_EVENT_DELETE ) return;
    if (!target_btn || target_btn == ring_container) return;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev && code != LV_EVENT_DELETE) return;

     if(indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) { is_dragging = true; }

    if (code == LV_EVENT_PRESSED) {
        is_dragging = false;
        if (inertia_timer) { lv_timer_del(inertia_timer); inertia_timer = NULL; }
        inertia_velocity = 0.0f;
    }
    else if (code == LV_EVENT_PRESSING && is_dragging) {
        lv_point_t vect; lv_indev_get_vect(indev, &vect);
        if (vect.x != 0 || vect.y != 0) {
            int button_index = -1;
            for (int i = 0; i < NUM_BUTTONS; ++i) if (ring_buttons[i] == target_btn) { button_index = i; break; }
            if (button_index == -1) return;
            const float angle_step = 2.0f * M_PI / NUM_BUTTONS;
            float button_base_angle = current_ring_angle + (float)button_index * angle_step;
            float tangent_x = -sinf(button_base_angle);
            float tangent_y = cosf(button_base_angle);
            float tangential_component = (float)vect.x * tangent_x + (float)vect.y * tangent_y;
            float delta_angle = tangential_component / RING_RADIUS;
            current_ring_angle += delta_angle * ROTATION_SENSITIVITY;
            inertia_velocity = delta_angle * ROTATION_SENSITIVITY;
            update_button_positions();
        } else { inertia_velocity = 0.0f; }
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // Reset drag flag immediately on release
        is_dragging = false; // <<< Reset directly here
        // Start inertia if needed
        if (fabsf(inertia_velocity) > INERTIA_STOP_THRESHOLD) {
            if (inertia_timer) { lv_timer_del(inertia_timer); inertia_timer = NULL; }
             inertia_timer = lv_timer_create(inertia_timer_cb, INERTIA_TIMER_PERIOD, NULL);
             if (!inertia_timer) { ESP_LOGE(TAG, "Failed inertia timer create!"); inertia_velocity = 0.0f; }
        } else { inertia_velocity = 0.0f; }
    }
}


// --- Cleanup callback when parent page is deleted ---
static void parent_delete_cb(lv_event_t *e) {
    // ... (Same as V12) ...
     lv_event_code_t code = lv_event_get_code(e);
     if (code == LV_EVENT_DELETE) {
         ESP_LOGI(TAG, "Parent object deleted. Cleaning up ring menu.");
         if (inertia_timer) { lv_timer_del(inertia_timer); inertia_timer = NULL; }
         for (int i = 0; i < NUM_BUTTONS; i++) ring_buttons[i] = NULL;
         ring_container = NULL;
         // Reset static styles if needed
         // lv_style_reset(&style_button_ring);
         // lv_style_reset(&style_page_bg);
     }
}

// --- Timer Callback for Initial Layout ---
static void ring_layout_timer_cb(lv_timer_t *timer) {
    // ... (Same as V11/V12) ...
    lv_obj_t *parent = (lv_obj_t*)timer->user_data;
    if (!parent || !lv_obj_is_valid(parent) || !ring_container || !lv_obj_is_valid(ring_container)) { return; }
    ESP_LOGI(TAG, "Executing ring_layout_timer_cb...");
    lv_coord_t parent_w = lv_obj_get_content_width(parent);
    lv_coord_t parent_h = lv_obj_get_content_height(parent);
    ring_center.x = parent_w / 2;
    ring_center.y = parent_h / 2;
    lv_area_t cont_coords; lv_obj_get_coords(ring_container, &cont_coords);
    container_screen_x = cont_coords.x1; container_screen_y = cont_coords.y1;
    ESP_LOGI(TAG, "Parent Content: %dx%d, Center: (%d, %d), Cont Screen: (%d, %d)", parent_w, parent_h, ring_center.x, ring_center.y, container_screen_x, container_screen_y);
     if (ring_center.y + RING_RADIUS + BUTTON_SIZE / 2 > parent_h || ring_center.y - RING_RADIUS - BUTTON_SIZE / 2 < 0 ) {
         ESP_LOGW(TAG, "Ring radius %d might be too large for content height %d!", RING_RADIUS, parent_h);
     }
    update_button_positions();
    ESP_LOGI(TAG, "Initial button layout complete.");
}

// --- Timer Callback for Inertia ---
static void inertia_timer_cb(lv_timer_t *timer) {
    // ... (Same as V12) ...
    current_ring_angle += inertia_velocity;
    update_button_positions();
    inertia_velocity *= INERTIA_DECEL_FACTOR;
    if (fabsf(inertia_velocity) < INERTIA_STOP_THRESHOLD) {
        lv_timer_del(timer);
        inertia_timer = NULL;
        inertia_velocity = 0.0f;
    }
}


// --- Initialize Styles ---
static void init_styles(void) {
    // Page Background Style
    lv_style_init(&style_page_bg);
    lv_style_set_bg_opa(&style_page_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_page_bg, lv_color_hex(0x2F4F4F)); // Dark Slate Gray

    // Button Default Style (Only style now)
    lv_style_init(&style_button_ring);
    lv_style_set_radius(&style_button_ring, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_button_ring, LV_OPA_COVER); // Ensure fully opaque
    lv_style_set_border_width(&style_button_ring, 1);
    lv_style_set_border_color(&style_button_ring, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_style_set_shadow_width(&style_button_ring, 8);
    lv_style_set_shadow_color(&style_button_ring, lv_color_hex3(0x333));
    lv_style_set_shadow_ofs_y(&style_button_ring, 4);
    lv_style_set_shadow_opa(&style_button_ring, LV_OPA_50);
    lv_style_set_text_color(&style_button_ring, lv_color_black());
    // Ensure default zoom is normal and no transition is applied
    lv_style_set_transform_zoom(&style_button_ring, 256);
    // --- REMOVED Transition ---
    // lv_style_set_transition(&style_button_ring, &trans_dsc);

    // --- REMOVED Pressed Style Definition ---
}


/**
 * @brief Initializes the UI elements for the "Circuits" page (Rotating Ring Menu V14 No Press FX).
 */
void page_07_circuits_init(lv_obj_t *parent) {
    ESP_LOGI(TAG, "Initializing Page 7: Circuits (Rotating Ring Menu V14 No Press FX)...");
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Init state
    // ... (Same as V12) ...
    for(int k=0; k<NUM_BUTTONS; ++k) ring_buttons[k] = NULL;
    current_ring_angle = 0.0f; ring_container = NULL;
    ring_center.x = 0; ring_center.y = 0; container_screen_x = 0; container_screen_y = 0;
    is_dragging = false;
    inertia_timer = NULL; inertia_velocity = 0.0f;


    // Initialize Styles
    init_styles(); // Initializes only base styles now

    // Apply Page Background Style
    lv_obj_add_style(parent, &style_page_bg, 0);

    // Create the Ring Container
    // ... (Same as V12) ...
     ring_container = lv_obj_create(parent);
    if (!ring_container) { ESP_LOGE(TAG, "Failed to create ring container!"); return; }
    lv_obj_set_size(ring_container, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(ring_container, 0, 0);
    lv_obj_set_style_bg_opa(ring_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring_container, 0, 0);
    lv_obj_set_style_pad_all(ring_container, 0, 0);
    // Container still doesn't need event handler

    // Create Buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
        ring_buttons[i] = create_ring_button(ring_container, i); // No longer adds pressed style
        if (!ring_buttons[i]) { ESP_LOGE(TAG, "Failed to create ring button %d!", i); }
    }

    // Add cleanup handler
    lv_obj_add_event_cb(parent, parent_delete_cb, LV_EVENT_DELETE, NULL);

    // Schedule Initial Layout
    // ... (Same as V12) ...
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