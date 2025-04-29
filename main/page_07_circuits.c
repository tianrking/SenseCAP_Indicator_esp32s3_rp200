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

static const char *TAG = "Page7_RingDial_V17_Independent"; // Updated Tag

// --- Configuration ---
#define NUM_OUTER_BUTTONS      10
#define OUTER_BUTTON_SIZE      60
#define OUTER_RING_RADIUS      170

#define NUM_INNER_BUTTONS      6
#define INNER_BUTTON_SIZE      50
#define INNER_RING_RADIUS      100

#define ROTATION_SENSITIVITY 1.3f
#define INERTIA_DECEL_FACTOR 0.93f
#define INERTIA_STOP_THRESHOLD 0.002f
#define INERTIA_TIMER_PERIOD 20

// --- Static variables ---
static lv_obj_t *ring_container = NULL;
static lv_obj_t *outer_ring_buttons[NUM_OUTER_BUTTONS];
static lv_obj_t *inner_ring_buttons[NUM_INNER_BUTTONS];
static lv_style_t style_button_ring;
static lv_style_t style_page_bg;
static float outer_ring_angle = 0.0f;
static float inner_ring_angle = 0.0f;
static bool is_dragging = false;
static lv_point_t ring_center = {0, 0};
static lv_coord_t container_screen_x = 0;
static lv_coord_t container_screen_y = 0;
static lv_timer_t *outer_inertia_timer = NULL;
static float outer_inertia_velocity = 0.0f;
static lv_timer_t *inner_inertia_timer = NULL;
static float inner_inertia_velocity = 0.0f;

// --- Color Palette ---
static const lv_color_t button_colors[] = {
    LV_COLOR_MAKE(0xFF, 0xB6, 0xC1), LV_COLOR_MAKE(0xAD, 0xD8, 0xE6), LV_COLOR_MAKE(0x98, 0xFB, 0x98),
    LV_COLOR_MAKE(0xFF, 0xFF, 0xE0), LV_COLOR_MAKE(0xFF, 0xD8, 0xB1), LV_COLOR_MAKE(0xE6, 0xE6, 0xFA),
    LV_COLOR_MAKE(0xAF, 0xEE, 0xEE), LV_COLOR_MAKE(0xFF, 0xE4, 0xE1), LV_COLOR_MAKE(0xBC, 0x8F, 0x8F),
    LV_COLOR_MAKE(0xF0, 0xE6, 0x8C)
};
#define NUM_COLORS (sizeof(button_colors) / sizeof(lv_color_t))

// User data structure
typedef struct {
    bool is_inner;
    int index;
    int id;
} ring_button_data_t;

// --- Forward declarations ---
static void button_click_cb(lv_event_t *e);
static lv_obj_t* create_ring_button(lv_obj_t *parent_obj, int index, int id, int size, bool is_inner);
static void parent_delete_cb(lv_event_t *e);
static void update_button_positions(void);
static void ring_drag_event_cb(lv_event_t *e);
static void ring_layout_timer_cb(lv_timer_t *timer);
static void init_styles(void);
static void outer_inertia_timer_cb(lv_timer_t *timer);
static void inner_inertia_timer_cb(lv_timer_t *timer);
static void free_button_user_data_cb(lv_event_t *e);


// --- Callback to free button user data on deletion ---
static void free_button_user_data_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        lv_obj_t* btn = lv_event_get_target(e);
        ring_button_data_t* btn_data = (ring_button_data_t*)lv_obj_get_user_data(btn);
        if (btn_data) { lv_mem_free(btn_data); }
    }
}

// --- Button Click Callback ---
static void button_click_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    ring_button_data_t* data = (ring_button_data_t*)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED && !is_dragging && data) {
        ESP_LOGI(TAG, "Ring Button ID %d (%s ring, index %d) clicked!",
                 data->id, data->is_inner ? "Inner" : "Outer", data->index);
    }
}

// --- Helper to create a single ring button ---
static lv_obj_t* create_ring_button(lv_obj_t *parent_obj, int index, int id, int button_size, bool is_inner) {
    lv_obj_t *btn = lv_btn_create(parent_obj);
    // ... (Same as V15b - size, style, color, label, user data allocation) ...
    if (!btn) return NULL;
    lv_obj_set_size(btn, button_size, button_size);
    lv_obj_add_style(btn, &style_button_ring, 0);
    lv_color_t color = button_colors[id % NUM_COLORS];
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_t *label = lv_label_create(btn);
    if (label) {
        char label_text[4]; sprintf(label_text, "%d", id); lv_label_set_text(label, label_text); lv_obj_center(label);
        if (lv_color_brightness(color) < 120) lv_obj_set_style_text_color(label, lv_color_white(), 0);
        else lv_obj_set_style_text_color(label, lv_color_black(), 0);
        lv_obj_set_style_pad_all(label, 0, 0);
    }
    ring_button_data_t* data = (ring_button_data_t*)lv_mem_alloc(sizeof(ring_button_data_t));
    if(data) {
        data->is_inner = is_inner; data->index = index; data->id = id;
        lv_obj_set_user_data(btn, data);
        lv_obj_add_event_cb(btn, button_click_cb, LV_EVENT_CLICKED, data);
        lv_obj_add_event_cb(btn, ring_drag_event_cb, LV_EVENT_ALL, data);
        lv_obj_add_event_cb(btn, free_button_user_data_cb, LV_EVENT_DELETE, NULL);
    } else {
        ESP_LOGE(TAG, "Failed user data alloc for button ID %d", id);
        lv_obj_add_event_cb(btn, button_click_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(btn, ring_drag_event_cb, LV_EVENT_ALL, NULL);
    }
    return btn;
}

// --- Update Button Positions ---
static void update_button_positions(void) {
    // ... (Same as V16 - uses separate inner/outer angles) ...
     if (ring_center.x == 0 && ring_center.y == 0) return;
    const float outer_angle_step = 2.0f * M_PI / NUM_OUTER_BUTTONS;
    for (int i = 0; i < NUM_OUTER_BUTTONS; i++) {
        if (!outer_ring_buttons[i] || !lv_obj_is_valid(outer_ring_buttons[i])) continue;
        float button_angle = fmodf(outer_ring_angle + (float)i * outer_angle_step, 2.0f * M_PI);
        lv_coord_t x = (lv_coord_t)(ring_center.x + OUTER_RING_RADIUS * cosf(button_angle) - OUTER_BUTTON_SIZE / 2);
        lv_coord_t y = (lv_coord_t)(ring_center.y + OUTER_RING_RADIUS * sinf(button_angle) - OUTER_BUTTON_SIZE / 2);
        lv_obj_set_pos(outer_ring_buttons[i], x, y);
    }
    const float inner_angle_step = 2.0f * M_PI / NUM_INNER_BUTTONS;
    for (int i = 0; i < NUM_INNER_BUTTONS; i++) {
        if (!inner_ring_buttons[i] || !lv_obj_is_valid(inner_ring_buttons[i])) continue;
        float button_angle = fmodf(inner_ring_angle + (float)i * inner_angle_step, 2.0f * M_PI);
        lv_coord_t x = (lv_coord_t)(ring_center.x + INNER_RING_RADIUS * cosf(button_angle) - INNER_BUTTON_SIZE / 2);
        lv_coord_t y = (lv_coord_t)(ring_center.y + INNER_RING_RADIUS * sinf(button_angle) - INNER_BUTTON_SIZE / 2);
        lv_obj_set_pos(inner_ring_buttons[i], x, y);
    }
}

// --- Ring Button Drag Event Callback ---
static void ring_drag_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target_btn = lv_event_get_target(e);
    ring_button_data_t* data = (ring_button_data_t*)lv_event_get_user_data(e);

    if (ring_center.x == 0 && ring_center.y == 0 && code != LV_EVENT_DELETE ) return;
    if (!target_btn || target_btn == ring_container || !data) return;

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev && code != LV_EVENT_DELETE) return;

     if(indev && lv_indev_get_gesture_dir(indev) != LV_DIR_NONE) { is_dragging = true; }

    if (code == LV_EVENT_PRESSED) {
        is_dragging = false;
        // --- FIX: Stop ONLY the inertia timer for the PRESSED ring ---
        if (data->is_inner) {
            if (inner_inertia_timer) {
                // ESP_LOGD(TAG, "Inner inertia stopped by press.");
                lv_timer_del(inner_inertia_timer);
                inner_inertia_timer = NULL;
            }
            inner_inertia_velocity = 0.0f;
        } else {
            if (outer_inertia_timer) {
                 // ESP_LOGD(TAG, "Outer inertia stopped by press.");
                 lv_timer_del(outer_inertia_timer);
                 outer_inertia_timer = NULL;
             }
            outer_inertia_velocity = 0.0f;
        }
    }
    else if (code == LV_EVENT_PRESSING && is_dragging) {
        // ... (Logic to calculate delta_angle based on tangential component and correct radius is same as V16) ...
        lv_point_t vect; lv_indev_get_vect(indev, &vect);
        if (vect.x != 0 || vect.y != 0) {
            int button_index = data->index;
            bool is_inner = data->is_inner;
            float angle_step = is_inner ? (2.0f * M_PI / NUM_INNER_BUTTONS) : (2.0f * M_PI / NUM_OUTER_BUTTONS);
            float radius = is_inner ? (float)INNER_RING_RADIUS : (float)OUTER_RING_RADIUS;
            float current_angle = is_inner ? inner_ring_angle : outer_ring_angle; // <<< Get correct current angle
            float button_base_angle = current_angle + (float)button_index * angle_step;
            float tangent_x = -sinf(button_base_angle);
            float tangent_y = cosf(button_base_angle);
            float tangential_component = (float)vect.x * tangent_x + (float)vect.y * tangent_y;
            float delta_angle = (radius > 1.0f) ? (tangential_component / radius) : 0;
            float velocity = delta_angle * ROTATION_SENSITIVITY;

            // Update the CORRECT angle and velocity
            if (is_inner) {
                inner_ring_angle += velocity;
                inner_inertia_velocity = velocity;
            } else {
                outer_ring_angle += velocity;
                outer_inertia_velocity = velocity;
            }
            update_button_positions(); // Update positions for BOTH rings
        } else {
             // No movement, clear potential inertia velocity for the dragged ring
             if (data->is_inner) inner_inertia_velocity = 0.0f;
             else outer_inertia_velocity = 0.0f;
        }
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        is_dragging = false;
        // --- Start the CORRECT inertia timer ---
        if (data->is_inner) {
            if (fabsf(inner_inertia_velocity) > INERTIA_STOP_THRESHOLD) {
                if (inner_inertia_timer) { lv_timer_del(inner_inertia_timer); inner_inertia_timer = NULL; }
                 inner_inertia_timer = lv_timer_create(inner_inertia_timer_cb, INERTIA_TIMER_PERIOD, NULL);
                 if (!inner_inertia_timer) { ESP_LOGE(TAG, "Failed inner inertia timer!"); inner_inertia_velocity = 0.0f; }
            } else { inner_inertia_velocity = 0.0f; }
        } else {
            if (fabsf(outer_inertia_velocity) > INERTIA_STOP_THRESHOLD) {
                 if (outer_inertia_timer) { lv_timer_del(outer_inertia_timer); outer_inertia_timer = NULL; }
                 outer_inertia_timer = lv_timer_create(outer_inertia_timer_cb, INERTIA_TIMER_PERIOD, NULL);
                 if (!outer_inertia_timer) { ESP_LOGE(TAG, "Failed outer inertia timer!"); outer_inertia_velocity = 0.0f; }
            } else { outer_inertia_velocity = 0.0f; }
        }
    }
}


// --- Cleanup callback when parent page is deleted ---
static void parent_delete_cb(lv_event_t *e) {
     // ... (Same as V16 - deletes both timers, clears both arrays) ...
     lv_event_code_t code = lv_event_get_code(e);
     if (code == LV_EVENT_DELETE) {
         ESP_LOGI(TAG, "Parent object deleted. Cleaning up ring menu.");
         if (outer_inertia_timer) { lv_timer_del(outer_inertia_timer); outer_inertia_timer = NULL; }
         if (inner_inertia_timer) { lv_timer_del(inner_inertia_timer); inner_inertia_timer = NULL; }
         for (int i = 0; i < NUM_OUTER_BUTTONS; i++) outer_ring_buttons[i] = NULL;
         for (int i = 0; i < NUM_INNER_BUTTONS; i++) inner_ring_buttons[i] = NULL;
         ring_container = NULL;
     }
}

// --- Timer Callback for Initial Layout ---
static void ring_layout_timer_cb(lv_timer_t *timer) {
    // ... (Same as V16 - creates both rings) ...
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
    if (ring_center.y + OUTER_RING_RADIUS + OUTER_BUTTON_SIZE / 2 > parent_h || ring_center.y - OUTER_RING_RADIUS - OUTER_BUTTON_SIZE / 2 < 0 ) {
         ESP_LOGW(TAG, "Outer Ring radius %d might be too large!", OUTER_RING_RADIUS);
     }

    ESP_LOGI(TAG, "Creating outer ring buttons...");
    for (int i = 0; i < NUM_OUTER_BUTTONS; i++) {
        outer_ring_buttons[i] = create_ring_button(ring_container, i, i + 1, OUTER_BUTTON_SIZE, false);
        if (!outer_ring_buttons[i]) ESP_LOGE(TAG, "Failed outer button %d", i);
    }
    ESP_LOGI(TAG, "Creating inner ring buttons...");
     for (int i = 0; i < NUM_INNER_BUTTONS; i++) {
        inner_ring_buttons[i] = create_ring_button(ring_container, i, NUM_OUTER_BUTTONS + i + 1, INNER_BUTTON_SIZE, true);
         if (!inner_ring_buttons[i]) ESP_LOGE(TAG, "Failed inner button %d", i);
    }
    update_button_positions();
    ESP_LOGI(TAG, "Initial button layout complete.");
}

// --- Timer Callback for Outer Ring Inertia ---
static void outer_inertia_timer_cb(lv_timer_t *timer) {
    // ... (Same as V16) ...
    outer_ring_angle += outer_inertia_velocity;
    update_button_positions();
    outer_inertia_velocity *= INERTIA_DECEL_FACTOR;
    if (fabsf(outer_inertia_velocity) < INERTIA_STOP_THRESHOLD) {
        lv_timer_del(timer);
        outer_inertia_timer = NULL;
        outer_inertia_velocity = 0.0f;
    }
}

// --- Timer Callback for Inner Ring Inertia ---
static void inner_inertia_timer_cb(lv_timer_t *timer) {
    // ... (Same as V16) ...
     inner_ring_angle += inner_inertia_velocity;
    update_button_positions();
    inner_inertia_velocity *= INERTIA_DECEL_FACTOR;
    if (fabsf(inner_inertia_velocity) < INERTIA_STOP_THRESHOLD) {
        lv_timer_del(timer);
        inner_inertia_timer = NULL;
        inner_inertia_velocity = 0.0f;
    }
}

// --- Initialize Styles ---
static void init_styles(void) {
    // ... (Same as V14 - Plain style) ...
    lv_style_init(&style_page_bg);
    lv_style_set_bg_opa(&style_page_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_page_bg, lv_color_hex(0x2F4F4F));
    lv_style_init(&style_button_ring);
    lv_style_set_radius(&style_button_ring, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_button_ring, LV_OPA_COVER);
    lv_style_set_border_width(&style_button_ring, 1);
    lv_style_set_border_color(&style_button_ring, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_style_set_shadow_width(&style_button_ring, 8);
    lv_style_set_shadow_color(&style_button_ring, lv_color_hex3(0x333));
    lv_style_set_shadow_ofs_y(&style_button_ring, 4);
    lv_style_set_shadow_opa(&style_button_ring, LV_OPA_50);
    lv_style_set_text_color(&style_button_ring, lv_color_black());
    lv_style_set_transform_zoom(&style_button_ring, 256);
}


/**
 * @brief Initializes the UI elements for the "Circuits" page (Dual Independent Rings V17).
 */
void page_07_circuits_init(lv_obj_t *parent) {
    // ... (Same as V16 - Init state, styles, container, cleanup cb, layout timer) ...
    ESP_LOGI(TAG, "Initializing Page 7: Circuits (Dual Independent Rings V17)...");
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    for(int k=0; k<NUM_OUTER_BUTTONS; ++k) outer_ring_buttons[k] = NULL;
    for(int k=0; k<NUM_INNER_BUTTONS; ++k) inner_ring_buttons[k] = NULL;
    outer_ring_angle = 0.0f; inner_ring_angle = 0.0f;
    ring_container = NULL;
    ring_center.x = 0; ring_center.y = 0; container_screen_x = 0; container_screen_y = 0;
    is_dragging = false;
    outer_inertia_timer = NULL; outer_inertia_velocity = 0.0f;
    inner_inertia_timer = NULL; inner_inertia_velocity = 0.0f;

    init_styles();
    lv_obj_add_style(parent, &style_page_bg, 0);

    ring_container = lv_obj_create(parent);
    if (!ring_container) { ESP_LOGE(TAG, "Failed ring container create!"); return; }
    lv_obj_set_size(ring_container, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(ring_container, 0, 0);
    lv_obj_set_style_bg_opa(ring_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring_container, 0, 0);
    lv_obj_set_style_pad_all(ring_container, 0, 0);

    lv_obj_add_event_cb(parent, parent_delete_cb, LV_EVENT_DELETE, NULL);

    lv_timer_t *layout_timer = lv_timer_create(ring_layout_timer_cb, 50, parent);
    if (layout_timer) {
        lv_timer_set_repeat_count(layout_timer, 1);
        ESP_LOGI(TAG, "Initial layout timer created.");
    } else {
        ESP_LOGE(TAG, "Failed layout timer create!");
         lv_obj_t * error_label = lv_label_create(parent);
         lv_label_set_text(error_label, "Error:\nLayout timer fail!");
         lv_obj_center(error_label);
    }

    ESP_LOGI(TAG, "Page 7 Init Complete (layout scheduled).");
}