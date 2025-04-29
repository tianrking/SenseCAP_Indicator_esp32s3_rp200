// my_ui.c
#include "my_ui.h"
#include "lvgl.h"
#include <math.h>   // For sinf, cosf, M_PI
#include <stdlib.h> // For rand, abs

// --- Configuration for 480x480 Screen ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

#define MAX_ELEMENTS 30  // H to Zn
#define MAX_ELECTRONS 30 // Max electrons for Zinc
#define MAX_SHELLS 4     // 4 shells needed up to Zn

// Adjusted sizes for 4 shells, shifted up
#define NUCLEUS_RADIUS 12 // Smaller nucleus
#define ELECTRON_RADIUS 5 // Smaller electrons
// *** REDUCED RADII to make diagram more compact vertically ***
const lv_coord_t element_radii[MAX_SHELLS] = {45, 75, 105, 135}; // Reduced gaps (30, 30, 30)
#define ORBIT_LINE_WIDTH 2

#define ANIMATION_TIME_MS 7500

// --- Data Structure for Elements (Expanded to 30) ---
typedef struct {
    const char *symbol;
    const char *name;
    uint8_t total_electrons;
    uint8_t electrons_per_shell[MAX_SHELLS]; // [Shell 1, 2, 3, 4]
} element_info_t;

// Data for the first 30 elements (H to Zn) - Same as previous
static const element_info_t elements[MAX_ELEMENTS] = {
    { "H",  "Hydrogen",  1, {1, 0, 0, 0}}, { "He", "Helium",    2, {2, 0, 0, 0}},
    { "Li", "Lithium",   3, {2, 1, 0, 0}}, { "Be", "Beryllium", 4, {2, 2, 0, 0}},
    { "B",  "Boron",     5, {2, 3, 0, 0}}, { "C",  "Carbon",    6, {2, 4, 0, 0}},
    { "N",  "Nitrogen",  7, {2, 5, 0, 0}}, { "O",  "Oxygen",    8, {2, 6, 0, 0}},
    { "F",  "Fluorine",  9, {2, 7, 0, 0}}, { "Ne", "Neon",     10, {2, 8, 0, 0}},
    { "Na", "Sodium",   11, {2, 8, 1, 0}}, { "Mg", "Magnesium",12, {2, 8, 2, 0}},
    { "Al", "Aluminum", 13, {2, 8, 3, 0}}, { "Si", "Silicon",  14, {2, 8, 4, 0}},
    { "P",  "Phosphorus",15,{2, 8, 5, 0}}, { "S",  "Sulfur",   16, {2, 8, 6, 0}},
    { "Cl", "Chlorine", 17, {2, 8, 7, 0}}, { "Ar", "Argon",    18, {2, 8, 8, 0}},
    { "K",  "Potassium",19, {2, 8, 8, 1}}, { "Ca", "Calcium",  20, {2, 8, 8, 2}},
    { "Sc", "Scandium", 21, {2, 8, 9, 2}}, { "Ti", "Titanium", 22, {2, 8, 10, 2}},
    { "V",  "Vanadium", 23, {2, 8, 11, 2}},{ "Cr", "Chromium", 24, {2, 8, 13, 1}},
    { "Mn", "Manganese",25, {2, 8, 13, 2}},{ "Fe", "Iron",     26, {2, 8, 14, 2}},
    { "Co", "Cobalt",   27, {2, 8, 15, 2}},{ "Ni", "Nickel",   28, {2, 8, 16, 2}},
    { "Cu", "Copper",   29, {2, 8, 18, 1}},{ "Zn", "Zinc",     30, {2, 8, 18, 2}}
};

// --- Static UI Variables ---
// (No changes needed here)
static lv_obj_t * animation_canvas;
static lv_obj_t * button_container;
static lv_obj_t * element_label;
static lv_obj_t * nucleus_obj;
static lv_obj_t * electron_objs[MAX_ELECTRONS];
static lv_obj_t * orbit_objs[MAX_SHELLS];
static lv_anim_t electron_anims[MAX_ELECTRONS];
static uint8_t current_element_index = 0;
static lv_coord_t center_x;
static lv_coord_t center_y; // Shifted UP center Y

// --- Forward Declarations ---
// (No changes needed here)
static void create_element_display(uint8_t element_index);
static void clear_element_display(void);
static void electron_anim_exec_cb(void * var, int32_t v);
static void element_select_event_cb(lv_event_t * e);

// --- Style Definitions ---
// (No changes needed here)
static lv_style_t style_nucleus;
static lv_style_t style_electron;
static lv_style_t style_shell_line;
static lv_style_t style_button;

// --- Function Implementations ---

/**
 * @brief Callback function for electron animation.
 */
static void electron_anim_exec_cb(void * var, int32_t v) {
    // (No changes needed)
    lv_obj_t * obj = (lv_obj_t *)var;
    lv_coord_t radius = (lv_coord_t)(lv_uintptr_t)lv_obj_get_user_data(obj);
    float angle_rad = (float)(v % 360) * M_PI / 180.0f;
    lv_coord_t x = center_x + (lv_coord_t)(radius * cosf(angle_rad)) - ELECTRON_RADIUS;
    lv_coord_t y = center_y + (lv_coord_t)(radius * sinf(angle_rad)) - ELECTRON_RADIUS;
    lv_obj_set_pos(obj, x, y);
}

/**
 * @brief Clears previous element's display artifacts.
 */
static void clear_element_display(void) {
    // (No changes needed)
    for (int i = 0; i < MAX_ELECTRONS; i++) {
        if (electron_objs[i]) {
            lv_anim_del(electron_objs[i], electron_anim_exec_cb);
            lv_obj_add_flag(electron_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    for (int i = 0; i < MAX_SHELLS; i++) {
        if (orbit_objs[i]) {
            lv_obj_add_flag(orbit_objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief Creates the visual representation for a given element.
 */
static void create_element_display(uint8_t element_index) {
    // This function's logic remains the same, it uses the updated radii
    // from the global element_radii array.
    if (element_index >= MAX_ELEMENTS) return;

    clear_element_display();

    const element_info_t *element = &elements[element_index];
    current_element_index = element_index;

    lv_label_set_text_fmt(element_label, "%s - %s", element->symbol, element->name);

    if (!nucleus_obj) {
        nucleus_obj = lv_obj_create(animation_canvas);
        lv_obj_remove_style_all(nucleus_obj);
        lv_obj_add_style(nucleus_obj, &style_nucleus, 0);
        // Use updated NUCLEUS_RADIUS
        lv_obj_set_size(nucleus_obj, NUCLEUS_RADIUS * 2, NUCLEUS_RADIUS * 2);
        lv_obj_align(nucleus_obj, LV_ALIGN_CENTER, center_x - SCREEN_WIDTH/2, center_y - SCREEN_HEIGHT/2);
    }
    lv_obj_clear_flag(nucleus_obj, LV_OBJ_FLAG_HIDDEN);


    // Create/Show Orbits using updated radii
    for (int shell = 0; shell < MAX_SHELLS; shell++) {
        if (element->electrons_per_shell[shell] > 0) {
            lv_coord_t radius = element_radii[shell]; // Use updated radii
             if (!orbit_objs[shell]) {
                 orbit_objs[shell] = lv_arc_create(animation_canvas);
                 lv_obj_remove_style(orbit_objs[shell], NULL, LV_PART_KNOB);
                 lv_obj_clear_flag(orbit_objs[shell], LV_OBJ_FLAG_CLICKABLE);
                 lv_obj_add_style(orbit_objs[shell], &style_shell_line, LV_PART_MAIN);
                 lv_obj_add_style(orbit_objs[shell], &style_shell_line, LV_PART_INDICATOR);
                 lv_arc_set_bg_angles(orbit_objs[shell], 0, 360);
                 lv_arc_set_angles(orbit_objs[shell], 0, 360);
             }
             lv_obj_set_size(orbit_objs[shell], radius * 2, radius * 2);
             lv_obj_align(orbit_objs[shell], LV_ALIGN_CENTER, center_x - SCREEN_WIDTH/2, center_y - SCREEN_HEIGHT/2);
             lv_obj_clear_flag(orbit_objs[shell], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Create/Show Electrons and Animations
    uint8_t electron_count = 0;
    int anim_direction = 1;
    for (int shell = 0; shell < MAX_SHELLS; shell++) {
        uint8_t shell_electrons = element->electrons_per_shell[shell];
        if (shell_electrons == 0) continue;

        lv_coord_t radius = element_radii[shell]; // Use updated radii
        float angle_step = (shell_electrons > 0) ? 360.0f / shell_electrons : 0;
        float start_angle_offset = (float)(shell * 45);
        anim_direction = 1;

        for (int i = 0; i < shell_electrons && electron_count < MAX_ELECTRONS; i++) {
             if (!electron_objs[electron_count]) {
                 electron_objs[electron_count] = lv_obj_create(animation_canvas);
                 lv_obj_remove_style_all(electron_objs[electron_count]);
                 lv_obj_add_style(electron_objs[electron_count], &style_electron, 0);
                 // Use updated ELECTRON_RADIUS
                 lv_obj_set_size(electron_objs[electron_count], ELECTRON_RADIUS * 2, ELECTRON_RADIUS * 2);
             }
             lv_obj_set_user_data(electron_objs[electron_count], (void*)(lv_uintptr_t)radius);
             lv_obj_clear_flag(electron_objs[electron_count], LV_OBJ_FLAG_HIDDEN);

            int32_t start_angle = (int32_t)(start_angle_offset + i * angle_step) % 360;
            int32_t end_angle = start_angle + (anim_direction * 359);

            lv_anim_init(&electron_anims[electron_count]);
            lv_anim_set_var(&electron_anims[electron_count], electron_objs[electron_count]);
            lv_anim_set_exec_cb(&electron_anims[electron_count], electron_anim_exec_cb);
            lv_anim_set_values(&electron_anims[electron_count], start_angle, end_angle);
            lv_anim_set_time(&electron_anims[electron_count], ANIMATION_TIME_MS + (rand() % 1000 - 500));
            lv_anim_set_playback_time(&electron_anims[electron_count], 0);
            lv_anim_set_repeat_count(&electron_anims[electron_count], LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&electron_anims[electron_count]);
            electron_count++;
        }
    }
}


/**
 * @brief Event callback for element selection buttons.
 */
static void element_select_event_cb(lv_event_t * e) {
    // (No changes needed)
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED) {
        uint8_t index = (uint8_t)(lv_uintptr_t)lv_obj_get_user_data(btn);
        if (index < MAX_ELEMENTS && index != current_element_index) {
           LV_LOG_USER("Button clicked for element index: %d", index);
           create_element_display(index);
        }
    }
}

/**
 * @brief Initialize UI: Animation shifted up, 30 buttons in row wrap layout below.
 */
void my_ui_init(void) {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_clean(screen);

    // --- Calculate Shifted Center ---
    center_x = SCREEN_WIDTH / 2;
    // *** Adjust Center Y slightly if needed, combined with smaller radii ***
    center_y = (lv_coord_t)(SCREEN_HEIGHT * 0.42); // e.g. 42% down, slightly lower than before
    LV_LOG_USER("Animation center: X=%d, Y=%d", (int)center_x, (int)center_y);

    // --- Initialize Styles ---
    // Update styles to use new smaller sizes
    lv_style_init(&style_nucleus);
    lv_style_set_radius(&style_nucleus, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_nucleus, LV_OPA_COVER);
    lv_style_set_bg_color(&style_nucleus, lv_palette_main(LV_PALETTE_DEEP_ORANGE));
    lv_style_set_border_width(&style_nucleus, 0);

    lv_style_init(&style_electron);
    lv_style_set_radius(&style_electron, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_electron, LV_OPA_COVER);
    lv_style_set_bg_color(&style_electron, lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 1));
    lv_style_set_border_width(&style_electron, 1); // Keep border
    lv_style_set_border_color(&style_electron, lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&style_shell_line);
    lv_style_set_arc_color(&style_shell_line, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_arc_width(&style_shell_line, ORBIT_LINE_WIDTH);
    lv_style_set_bg_opa(&style_shell_line, LV_OPA_TRANSP);
    lv_style_set_arc_rounded(&style_shell_line, false);

    lv_style_init(&style_button);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_bg_color(&style_button, lv_palette_main(LV_PALETTE_BLUE_GREY));
    lv_style_set_border_width(&style_button, 1);
    lv_style_set_border_color(&style_button, lv_palette_lighten(LV_PALETTE_BLUE_GREY, 2));
    lv_style_set_text_color(&style_button, lv_color_white());
    lv_style_set_pad_ver(&style_button, 8);
    lv_style_set_pad_hor(&style_button, 5);


    // --- Create Full Screen Canvas for Animation ---
    animation_canvas = lv_obj_create(screen);
    lv_obj_remove_style_all(animation_canvas);
    lv_obj_set_size(animation_canvas, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_align(animation_canvas, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(animation_canvas, lv_palette_darken(LV_PALETTE_GREY, 4), 0);
    lv_obj_set_style_bg_opa(animation_canvas, LV_OPA_COVER, 0);


    // --- Create Element Name Label (Overlay on Top) ---
    element_label = lv_label_create(animation_canvas);
    lv_obj_set_width(element_label, lv_pct(90));
    lv_obj_set_style_text_align(element_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(element_label, lv_color_white(), 0);
    lv_obj_align(element_label, LV_ALIGN_TOP_MID, 0, 10);


    // --- Create Button Container (Row Wrap Layout - Overlay on Bottom) ---
    button_container = lv_obj_create(animation_canvas);
    lv_obj_remove_style_all(button_container);
    lv_obj_set_width(button_container, lv_pct(95));
    // *** Reduce button container height slightly to give animation more space ***
    lv_obj_set_height(button_container, lv_pct(30)); // e.g., 30% height

    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                                         LV_FLEX_ALIGN_CENTER,
                                         LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_container, 6, 0);

    // *** Adjust alignment offset if container height changed ***
    lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, -10); // Keep 10px from bottom


    // Create buttons for the first 30 elements
    for (uint8_t i = 0; i < MAX_ELEMENTS; i++) { // Loop up to 30
        lv_obj_t * btn = lv_btn_create(button_container);
        lv_obj_add_style(btn, &style_button, 0);
        lv_obj_set_user_data(btn, (void*)(lv_uintptr_t)i);
        lv_obj_add_event_cb(btn, element_select_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, elements[i].symbol);
        lv_obj_center(lbl);
    }


    // --- Final Steps ---
    nucleus_obj = NULL;
    for(int i=0; i<MAX_ELECTRONS; i++) electron_objs[i] = NULL;
    for(int i=0; i<MAX_SHELLS; i++) orbit_objs[i] = NULL;


    // --- Display the first element initially ---
    create_element_display(0);

    LV_LOG_USER("Custom UI Initialized: 30 Elements, Row Wrap Buttons, Compact Animation.");
}