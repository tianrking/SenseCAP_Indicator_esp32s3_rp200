// my_ui.c
#include "my_ui.h"
#include "lvgl.h"
#include <math.h>   // For sinf, cosf, M_PI, sqrtf
#include <stdlib.h> // For rand, abs

// --- Configuration ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

#define MAX_ELEMENTS 44  // H to Ru
#define MAX_ELECTRONS 44 // Max electrons for Ruthenium
#define MAX_SHELLS 5     // 5 shells needed up to Ru

#define NUCLEUS_RADIUS 10
#define ELECTRON_RADIUS 4
// Compact RADII for 5 shells to avoid overlap
const lv_coord_t element_radii[MAX_SHELLS] = {35, 55, 75, 95, 115}; // Max radius 115
#define ORBIT_LINE_WIDTH 2

#define ANIMATION_TIME_MS 8000
#define FADE_TIME_MS 300 // Duration for fade in/out animations

// --- Data Structure for Elements ---
typedef struct {
    const char *symbol;
    const char *name;
    uint8_t total_electrons;
    uint8_t electrons_per_shell[MAX_SHELLS]; // [Shell 1, 2, 3, 4, 5]
} element_info_t;

// Data for the first 44 elements (H to Ru)
static const element_info_t elements[MAX_ELEMENTS] = {
    // Symbol, Name,      e-, Config[1, 2, 3, 4, 5]
    { "H",  "Hydrogen",  1, {1, 0, 0, 0, 0}}, { "He", "Helium",    2, {2, 0, 0, 0, 0}},
    { "Li", "Lithium",   3, {2, 1, 0, 0, 0}}, { "Be", "Beryllium", 4, {2, 2, 0, 0, 0}},
    { "B",  "Boron",     5, {2, 3, 0, 0, 0}}, { "C",  "Carbon",    6, {2, 4, 0, 0, 0}},
    { "N",  "Nitrogen",  7, {2, 5, 0, 0, 0}}, { "O",  "Oxygen",    8, {2, 6, 0, 0, 0}},
    { "F",  "Fluorine",  9, {2, 7, 0, 0, 0}}, { "Ne", "Neon",     10, {2, 8, 0, 0, 0}},
    { "Na", "Sodium",   11, {2, 8, 1, 0, 0}}, { "Mg", "Magnesium",12, {2, 8, 2, 0, 0}},
    { "Al", "Aluminum", 13, {2, 8, 3, 0, 0}}, { "Si", "Silicon",  14, {2, 8, 4, 0, 0}},
    { "P",  "Phosphorus",15,{2, 8, 5, 0, 0}}, { "S",  "Sulfur",   16, {2, 8, 6, 0, 0}},
    { "Cl", "Chlorine", 17, {2, 8, 7, 0, 0}}, { "Ar", "Argon",    18, {2, 8, 8, 0, 0}},
    { "K",  "Potassium",19, {2, 8, 8, 1, 0}}, { "Ca", "Calcium",  20, {2, 8, 8, 2, 0}},
    { "Sc", "Scandium", 21, {2, 8, 9, 2, 0}}, { "Ti", "Titanium", 22, {2, 8, 10, 2, 0}},
    { "V",  "Vanadium", 23, {2, 8, 11, 2, 0}},{ "Cr", "Chromium", 24, {2, 8, 13, 1, 0}}, // Exc
    { "Mn", "Manganese",25, {2, 8, 13, 2, 0}},{ "Fe", "Iron",     26, {2, 8, 14, 2, 0}},
    { "Co", "Cobalt",   27, {2, 8, 15, 2, 0}},{ "Ni", "Nickel",   28, {2, 8, 16, 2, 0}},
    { "Cu", "Copper",   29, {2, 8, 18, 1, 0}},{ "Zn", "Zinc",     30, {2, 8, 18, 2, 0}}, // Exc
    { "Ga", "Gallium",  31, {2, 8, 18, 3, 0}},{ "Ge", "Germanium",32, {2, 8, 18, 4, 0}},
    { "As", "Arsenic",  33, {2, 8, 18, 5, 0}},{ "Se", "Selenium", 34, {2, 8, 18, 6, 0}},
    { "Br", "Bromine",  35, {2, 8, 18, 7, 0}},{ "Kr", "Krypton",  36, {2, 8, 18, 8, 0}},
    { "Rb", "Rubidium", 37, {2, 8, 18, 8, 1}}, { "Sr", "Strontium",38, {2, 8, 18, 8, 2}},
    { "Y",  "Yttrium",  39, {2, 8, 18, 9, 2}}, { "Zr", "Zirconium",40, {2, 8, 18, 10, 2}},
    { "Nb", "Niobium",  41, {2, 8, 18, 12, 1}},{ "Mo", "Molybdenum",42,{2, 8, 18, 13, 1}},// Exc
    { "Tc", "Technetium",43,{2, 8, 18, 13, 2}},{ "Ru", "Ruthenium",44, {2, 8, 18, 15, 1}} // Exc
};

// --- Static UI Variables ---
static lv_obj_t * animation_canvas;
static lv_obj_t * button_container;
static lv_obj_t * element_label;
static lv_obj_t * nucleus_obj;
static lv_obj_t * electron_objs[MAX_ELECTRONS];
static lv_obj_t * orbit_objs[MAX_SHELLS];
static lv_anim_t electron_anims[MAX_ELECTRONS];
static uint8_t current_element_index = 0;
static lv_coord_t center_x;
static lv_coord_t center_y;

// --- Forward Declarations ---
static void create_element_display(uint8_t element_index);
static void start_fade_out_old_element(void);
static void electron_anim_exec_cb(void * var, int32_t v);
static void element_select_event_cb(lv_event_t * e);
static void fade_anim_opa_exec_cb(void * var, int32_t v);
static void fade_out_anim_ready_cb(lv_anim_t *a);

// --- Style Definitions ---
static lv_style_t style_nucleus;
static lv_style_t style_electron;
static lv_style_t style_shell_line;
static lv_style_t style_button;
static lv_style_t style_button_pressed;
static lv_style_t style_canvas_bg;

static const lv_style_prop_t btn_trans_props[] = {LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, LV_STYLE_PROP_INV};
static lv_style_transition_dsc_t btn_trans;

// --- Function Implementations ---

/** @brief Opacity animation callback */
static void fade_anim_opa_exec_cb(void * var, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)var, v, 0); }

/** @brief Electron position animation callback */
static void electron_anim_exec_cb(void * var, int32_t v) {
    lv_obj_t * obj = (lv_obj_t *)var;
    if(!lv_obj_is_valid(obj)) return;
    lv_coord_t radius = (lv_coord_t)(lv_uintptr_t)lv_obj_get_user_data(obj);
    float angle_rad = (float)(v % 360) * M_PI / 180.0f;
    lv_coord_t x = center_x + (lv_coord_t)(radius * cosf(angle_rad)) - ELECTRON_RADIUS;
    lv_coord_t y = center_y + (lv_coord_t)(radius * sinf(angle_rad)) - ELECTRON_RADIUS;
    lv_obj_set_pos(obj, x, y);
}

/** @brief Hide object after fade out */
static void fade_out_anim_ready_cb(lv_anim_t *a) {
    lv_obj_t * obj = (lv_obj_t *)a->var;
    if (lv_obj_is_valid(obj)) {
         lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

/** @brief Start fade out animations */
static void start_fade_out_old_element(void) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_time(&a, FADE_TIME_MS);
    lv_anim_set_exec_cb(&a, fade_anim_opa_exec_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_ready_cb(&a, fade_out_anim_ready_cb);
    lv_anim_set_deleted_cb(&a, NULL);

    for (int i = 0; i < MAX_ELECTRONS; i++) {
        if (electron_objs[i] && lv_obj_is_valid(electron_objs[i]) && !lv_obj_has_flag(electron_objs[i], LV_OBJ_FLAG_HIDDEN)) {
             lv_anim_del(electron_objs[i], fade_anim_opa_exec_cb);
             a.var = electron_objs[i];
             lv_anim_start(&a);
        }
    }
    for (int i = 0; i < MAX_SHELLS; i++) {
        if (orbit_objs[i] && lv_obj_is_valid(orbit_objs[i]) && !lv_obj_has_flag(orbit_objs[i], LV_OBJ_FLAG_HIDDEN)) {
             lv_anim_del(orbit_objs[i], fade_anim_opa_exec_cb);
             a.var = orbit_objs[i];
             lv_anim_start(&a);
        }
    }
}

/** @brief Create display (handles 5 shells) */
static void create_element_display(uint8_t element_index) {
    if (element_index >= MAX_ELEMENTS) return;

    start_fade_out_old_element();

    const element_info_t *element = &elements[element_index];
    current_element_index = element_index;
    lv_label_set_text_fmt(element_label, "%s - %s", element->symbol, element->name);

    if (!nucleus_obj) {
        nucleus_obj = lv_obj_create(animation_canvas);
        lv_obj_remove_style_all(nucleus_obj);
        lv_obj_add_style(nucleus_obj, &style_nucleus, 0);
        lv_obj_set_size(nucleus_obj, NUCLEUS_RADIUS * 2, NUCLEUS_RADIUS * 2);
        lv_obj_align(nucleus_obj, LV_ALIGN_CENTER, center_x - SCREEN_WIDTH/2, center_y - SCREEN_HEIGHT/2);
    }
    lv_obj_clear_flag(nucleus_obj, LV_OBJ_FLAG_HIDDEN);


    lv_anim_t a_fade_in;
    lv_anim_init(&a_fade_in);
    lv_anim_set_time(&a_fade_in, FADE_TIME_MS);
    lv_anim_set_delay(&a_fade_in, 50);
    lv_anim_set_exec_cb(&a_fade_in, fade_anim_opa_exec_cb);
    lv_anim_set_values(&a_fade_in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_ready_cb(&a_fade_in, NULL);
    lv_anim_set_deleted_cb(&a_fade_in, NULL);

    for (int shell = 0; shell < MAX_SHELLS; shell++) {
        if (element->electrons_per_shell[shell] > 0) {
            lv_coord_t radius = element_radii[shell];
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

             lv_obj_set_style_opa(orbit_objs[shell], LV_OPA_TRANSP, 0);
             lv_obj_clear_flag(orbit_objs[shell], LV_OBJ_FLAG_HIDDEN);
             lv_anim_del(orbit_objs[shell], fade_anim_opa_exec_cb);
             a_fade_in.var = orbit_objs[shell];
             lv_anim_start(&a_fade_in);
        }
    }

    uint8_t electron_count = 0;
    int anim_direction = 1;
    for (int shell = 0; shell < MAX_SHELLS; shell++) {
        uint8_t shell_electrons = element->electrons_per_shell[shell];
        if (shell_electrons == 0) continue;

        lv_coord_t radius = element_radii[shell];
        float angle_step = (shell_electrons > 0) ? 360.0f / shell_electrons : 0;
        float start_angle_offset = (float)(shell * 40);
        anim_direction = 1;

        for (int i = 0; i < shell_electrons && electron_count < MAX_ELECTRONS; i++) {
             if (!electron_objs[electron_count]) {
                 electron_objs[electron_count] = lv_obj_create(animation_canvas);
                 lv_obj_remove_style_all(electron_objs[electron_count]);
                 lv_obj_add_style(electron_objs[electron_count], &style_electron, 0);
                 lv_obj_set_size(electron_objs[electron_count], ELECTRON_RADIUS * 2, ELECTRON_RADIUS * 2);
             }
             lv_obj_set_user_data(electron_objs[electron_count], (void*)(lv_uintptr_t)radius);

             lv_obj_set_style_opa(electron_objs[electron_count], LV_OPA_TRANSP, 0);
             lv_obj_clear_flag(electron_objs[electron_count], LV_OBJ_FLAG_HIDDEN);
             lv_anim_del(electron_objs[electron_count], fade_anim_opa_exec_cb);
             a_fade_in.var = electron_objs[electron_count];
             lv_anim_start(&a_fade_in);


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

/** @brief Button click event callback */
static void element_select_event_cb(lv_event_t * e) {
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
 * @brief Initialize UI: Tech color theme, transitions.
 */
void my_ui_init(void) {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_clean(screen);

    center_x = SCREEN_WIDTH / 2;
    center_y = (lv_coord_t)(SCREEN_HEIGHT * 0.37); // Keep shifted up center

    // --- Initialize Styles (Tech Theme) ---
    // Background Style (Solid Dark Blue-Grey)
    lv_style_init(&style_canvas_bg);
    lv_style_set_bg_opa(&style_canvas_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&style_canvas_bg, lv_color_hex(0x101820)); // Set solid color

    // Nucleus Style (Bright Blue)
    lv_style_init(&style_nucleus);
    lv_style_set_radius(&style_nucleus, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_nucleus, LV_OPA_COVER);
    lv_style_set_bg_color(&style_nucleus, lv_palette_lighten(LV_PALETTE_BLUE, 1));
    lv_style_set_border_width(&style_nucleus, 0);

    // Electron Style (Cyan with Glow)
    lv_style_init(&style_electron);
    lv_style_set_radius(&style_electron, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_electron, LV_OPA_COVER);
    lv_style_set_bg_color(&style_electron, lv_palette_main(LV_PALETTE_CYAN));
    lv_style_set_border_width(&style_electron, 0);
    lv_style_set_shadow_width(&style_electron, 8);
    lv_style_set_shadow_spread(&style_electron, 2);
    lv_style_set_shadow_color(&style_electron, lv_palette_lighten(LV_PALETTE_CYAN, 1));
    lv_style_set_shadow_opa(&style_electron, LV_OPA_80);

    // Orbit Line Style (Muted Blue-Grey)
    lv_style_init(&style_shell_line);
    lv_style_set_arc_color(&style_shell_line, lv_color_hex(0x506070));
    lv_style_set_arc_width(&style_shell_line, ORBIT_LINE_WIDTH);
    lv_style_set_bg_opa(&style_shell_line, LV_OPA_TRANSP);
    lv_style_set_arc_rounded(&style_shell_line, true);

    // Button Styles (Grey/Blue Tech Theme)
    lv_style_transition_dsc_init(&btn_trans, btn_trans_props, lv_anim_path_ease_out, 100, 0, NULL);
    lv_style_init(&style_button);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_bg_color(&style_button, lv_color_hex(0x2A3B4D));
    lv_style_set_bg_grad_color(&style_button, lv_color_hex(0x4A5B6D));
    lv_style_set_bg_grad_dir(&style_button, LV_GRAD_DIR_VER);
    lv_style_set_border_width(&style_button, 1);
    lv_style_set_border_color(&style_button, lv_color_hex(0x6A7B8D));
    lv_style_set_text_color(&style_button, lv_color_white());
    lv_style_set_pad_all(&style_button, 7);
    lv_style_set_transition(&style_button, &btn_trans);

    lv_style_init(&style_button_pressed);
    lv_style_set_transform_width(&style_button_pressed, -2);
    lv_style_set_transform_height(&style_button_pressed, -2);
    lv_style_set_bg_color(&style_button_pressed, lv_color_hex(0x1A2B3D));
    lv_style_set_bg_grad_color(&style_button_pressed, lv_color_hex(0x3A4B5D));


    // --- Create Full Screen Canvas ---
    animation_canvas = lv_obj_create(screen);
    lv_obj_remove_style_all(animation_canvas);
    lv_obj_add_style(animation_canvas, &style_canvas_bg, 0); // Apply solid BG style
    lv_obj_set_size(animation_canvas, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_align(animation_canvas, LV_ALIGN_CENTER, 0, 0);


    // --- Create Element Name Label ---
    element_label = lv_label_create(animation_canvas);
    lv_obj_set_width(element_label, lv_pct(90));
    lv_obj_set_style_text_align(element_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(element_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_font(element_label, &lv_font_montserrat_18, 0);
    lv_obj_align(element_label, LV_ALIGN_TOP_MID, 0, 15);


    // --- Create Button Container ---
    button_container = lv_obj_create(animation_canvas);
    lv_obj_remove_style_all(button_container);
    lv_obj_set_width(button_container, lv_pct(96));
    lv_obj_set_height(button_container, lv_pct(35)); // Keep height for 44 buttons

    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                                         LV_FLEX_ALIGN_CENTER,
                                         LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_container, 5, 0);

    lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, -10);


    // Create buttons for the first 44 elements with new styles
    for (uint8_t i = 0; i < MAX_ELEMENTS; i++) { // Loop up to 44
        lv_obj_t * btn = lv_btn_create(button_container);
        lv_obj_remove_style(btn, NULL, LV_PART_MAIN);
        lv_obj_add_style(btn, &style_button, 0);
        lv_obj_add_style(btn, &style_button_pressed, LV_STATE_PRESSED);
        lv_obj_set_user_data(btn, (void*)(lv_uintptr_t)i);
        lv_obj_add_event_cb(btn, element_select_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, elements[i].symbol);
        lv_obj_center(lbl);
    }


    // --- Pre-allocate objects ---
    nucleus_obj = NULL;
    for(int i=0; i<MAX_ELECTRONS; i++) {
        electron_objs[i] = lv_obj_create(animation_canvas);
        lv_obj_remove_style_all(electron_objs[i]);
        lv_obj_add_style(electron_objs[i], &style_electron, 0);
        lv_obj_set_size(electron_objs[i], ELECTRON_RADIUS * 2, ELECTRON_RADIUS * 2);
        lv_obj_add_flag(electron_objs[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    }
     for(int i=0; i<MAX_SHELLS; i++) {
        orbit_objs[i] = lv_arc_create(animation_canvas);
        lv_obj_remove_style(orbit_objs[i], NULL, LV_PART_KNOB);
        lv_obj_clear_flag(orbit_objs[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_style(orbit_objs[i], &style_shell_line, LV_PART_MAIN);
        lv_obj_add_style(orbit_objs[i], &style_shell_line, LV_PART_INDICATOR);
        lv_arc_set_bg_angles(orbit_objs[i], 0, 360);
        lv_arc_set_angles(orbit_objs[i], 0, 360);
        lv_obj_add_flag(orbit_objs[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
     }


    // --- Display the first element initially ---
    create_element_display(0);

    LV_LOG_USER("Custom UI Initialized: 44 Elements, Tech Theme Colors V2.");
}