// my_ui.c
#include "my_ui.h"
#include "lvgl.h"
#include <math.h>   // For sinf, cosf, M_PI, sqrtf
#include <stdlib.h> // For rand, abs
#include <stdio.h>  // For sprintf

// --- Configuration ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

#define MAX_ELEMENTS 44  // H to Ru
#define MAX_ELECTRONS 44 // Max electrons for Ruthenium
#define MAX_SHELLS 5     // 5 shells needed up to Ru

// Sizes adjusted for potentially smaller animation area & look
#define NUCLEUS_RADIUS 10
#define ELECTRON_RADIUS 4
const lv_coord_t element_radii[MAX_SHELLS] = {35, 55, 75, 95, 115}; // Compact radii
#define ORBIT_LINE_WIDTH 1

#define ANIMATION_TIME_MS 8000
#define FADE_TIME_MS 300 // Duration for fade in/out animations

// --- Data Structure for Elements ---
typedef struct {
    const char *symbol;
    const char *name;
    uint8_t total_electrons; // Atomic Number Z = index + 1
    uint8_t electrons_per_shell[MAX_SHELLS];
    float atomic_mass;
    const char *config_str; // Electron configuration string
    const char *category;   // Element category
} element_info_t;

// Data for the first 44 elements (H to Ru)
static const element_info_t elements[MAX_ELEMENTS] = {
    // Symbol, Name,      e-, Config[1..5], Mass, Config Str (Linear), Category
    { "H",  "Hydrogen",  1, {1,0,0,0,0}, 1.008,  "1s1",       "Nonmetal"},
    { "He", "Helium",    2, {2,0,0,0,0}, 4.0026, "1s2",       "Noble Gas"},
    { "Li", "Lithium",   3, {2,1,0,0,0}, 6.94,   "[He] 2s1",  "Alkali Metal"},
    { "Be", "Beryllium", 4, {2,2,0,0,0}, 9.0122, "[He] 2s2",  "Alkaline Earth"},
    { "B",  "Boron",     5, {2,3,0,0,0}, 10.81,  "[He] 2s2 2p1","Metalloid"},
    { "C",  "Carbon",    6, {2,4,0,0,0}, 12.011, "[He] 2s2 2p2","Nonmetal"},
    { "N",  "Nitrogen",  7, {2,5,0,0,0}, 14.007, "[He] 2s2 2p3","Nonmetal"},
    { "O",  "Oxygen",    8, {2,6,0,0,0}, 15.999, "[He] 2s2 2p4","Nonmetal"},
    { "F",  "Fluorine",  9, {2,7,0,0,0}, 18.998, "[He] 2s2 2p5","Halogen"},
    { "Ne", "Neon",     10, {2,8,0,0,0}, 20.180, "[He] 2s2 2p6","Noble Gas"},
    { "Na", "Sodium",   11, {2,8,1,0,0}, 22.990, "[Ne] 3s1",  "Alkali Metal"},
    { "Mg", "Magnesium",12, {2,8,2,0,0}, 24.305, "[Ne] 3s2",  "Alkaline Earth"},
    { "Al", "Aluminum", 13, {2,8,3,0,0}, 26.982, "[Ne] 3s2 3p1","Post-transition Metal"},
    { "Si", "Silicon",  14, {2,8,4,0,0}, 28.085, "[Ne] 3s2 3p2","Metalloid"},
    { "P",  "Phosphorus",15,{2,8,5,0,0}, 30.974, "[Ne] 3s2 3p3","Nonmetal"},
    { "S",  "Sulfur",   16, {2,8,6,0,0}, 32.06,  "[Ne] 3s2 3p4","Nonmetal"},
    { "Cl", "Chlorine", 17, {2,8,7,0,0}, 35.45,  "[Ne] 3s2 3p5","Halogen"},
    { "Ar", "Argon",    18, {2,8,8,0,0}, 39.948, "[Ne] 3s2 3p6","Noble Gas"},
    { "K",  "Potassium",19, {2,8,8,1,0}, 39.098, "[Ar] 4s1",  "Alkali Metal"},
    { "Ca", "Calcium",  20, {2,8,8,2,0}, 40.078, "[Ar] 4s2",  "Alkaline Earth"},
    { "Sc", "Scandium", 21, {2,8,9,2,0}, 44.956, "[Ar] 3d1 4s2","Transition Metal"},
    { "Ti", "Titanium", 22, {2,8,10,2,0},47.867, "[Ar] 3d2 4s2","Transition Metal"},
    { "V",  "Vanadium", 23, {2,8,11,2,0},50.942, "[Ar] 3d3 4s2","Transition Metal"},
    { "Cr", "Chromium", 24, {2,8,13,1,0},51.996, "[Ar] 3d5 4s1","Transition Metal"},
    { "Mn", "Manganese",25, {2,8,13,2,0},54.938, "[Ar] 3d5 4s2","Transition Metal"},
    { "Fe", "Iron",     26, {2,8,14,2,0},55.845, "[Ar] 3d6 4s2","Transition Metal"},
    { "Co", "Cobalt",   27, {2,8,15,2,0},58.933, "[Ar] 3d7 4s2","Transition Metal"},
    { "Ni", "Nickel",   28, {2,8,16,2,0},58.693, "[Ar] 3d8 4s2","Transition Metal"},
    { "Cu", "Copper",   29, {2,8,18,1,0},63.546, "[Ar] 3d10 4s1","Transition Metal"},
    { "Zn", "Zinc",     30, {2,8,18,2,0},65.38,  "[Ar] 3d10 4s2","Post-transition Metal"},
    { "Ga", "Gallium",  31, {2,8,18,3,0},69.723, "[Ar] 3d10 4s2 4p1","Post-transition Metal"},
    { "Ge", "Germanium",32, {2,8,18,4,0},72.63,  "[Ar] 3d10 4s2 4p2","Metalloid"},
    { "As", "Arsenic",  33, {2,8,18,5,0},74.922, "[Ar] 3d10 4s2 4p3","Metalloid"},
    { "Se", "Selenium", 34, {2,8,18,6,0},78.971, "[Ar] 3d10 4s2 4p4","Nonmetal"},
    { "Br", "Bromine",  35, {2,8,18,7,0},79.904, "[Ar] 3d10 4s2 4p5","Halogen"},
    { "Kr", "Krypton",  36, {2,8,18,8,0},83.798, "[Ar] 3d10 4s2 4p6","Noble Gas"},
    { "Rb", "Rubidium", 37, {2,8,18,8,1},85.468, "[Kr] 5s1",  "Alkali Metal"},
    { "Sr", "Strontium",38, {2,8,18,8,2},87.62,  "[Kr] 5s2",  "Alkaline Earth"},
    { "Y",  "Yttrium",  39, {2,8,18,9,2},88.906, "[Kr] 4d1 5s2","Transition Metal"},
    { "Zr", "Zirconium",40, {2,8,18,10,2},91.224, "[Kr] 4d2 5s2","Transition Metal"},
    { "Nb", "Niobium",  41, {2,8,18,12,1},92.906, "[Kr] 4d4 5s1","Transition Metal"},
    { "Mo", "Molybdenum",42,{2,8,18,13,1},95.96,  "[Kr] 4d5 5s1","Transition Metal"},
    { "Tc", "Technetium",43,{2,8,18,13,2},(98),  "[Kr] 4d5 5s2","Transition Metal"},
    { "Ru", "Ruthenium",44, {2,8,18,15,1},101.07, "[Kr] 4d7 5s1","Transition Metal"}
};


// --- Static UI Variables ---
static lv_obj_t * main_container_top;
static lv_obj_t * animation_container;
static lv_obj_t * info_panel;
static lv_obj_t * button_container;
static lv_obj_t * element_label;
static lv_obj_t * nucleus_obj;
static lv_obj_t * electron_objs[MAX_ELECTRONS];
static lv_obj_t * orbit_objs[MAX_SHELLS];
static lv_anim_t electron_anims[MAX_ELECTRONS];
static lv_obj_t * label_info_title;
static lv_obj_t * label_atomic_num_lbl;
static lv_obj_t * label_atomic_num_val;
static lv_obj_t * label_atomic_mass_lbl;
static lv_obj_t * label_atomic_mass_val;
static lv_obj_t * label_config_lbl;
static lv_obj_t * label_config_val;
static lv_obj_t * label_category_lbl;
static lv_obj_t * label_category_val;
static uint8_t current_element_index = 0;
static lv_coord_t center_x; // Relative to animation_container
static lv_coord_t center_y; // Relative to animation_container, Adjusted Vertically

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
static lv_style_t style_canvas_bg; // Main background
static lv_style_t style_info_panel;
static lv_style_t style_info_label_title;
static lv_style_t style_info_label_static;
static lv_style_t style_info_label_value;

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
     if (nucleus_obj && lv_obj_is_valid(nucleus_obj)) {
        lv_obj_clear_flag(nucleus_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(nucleus_obj, LV_OPA_COVER, 0);
     }
}

/** @brief Create display (handles 5 shells, updates info panel, uses set_pos) */
static void create_element_display(uint8_t element_index) {
    if (element_index >= MAX_ELEMENTS) return;

    start_fade_out_old_element();

    const element_info_t *element = &elements[element_index];
    current_element_index = element_index;

    // Update Info Panel
    char buffer[64];
    lv_label_set_text_fmt(label_atomic_num_val, "%d", element_index + 1);
    sprintf(buffer, "%.3f u", element->atomic_mass);
    lv_label_set_text(label_atomic_mass_val, buffer);
    lv_label_set_text(label_config_val, element->config_str);
    lv_label_set_text(label_category_val, element->category);

    // Update Main Label
    lv_label_set_text_fmt(element_label, "%s - %s", element->symbol, element->name);
    lv_obj_align(element_label, LV_ALIGN_TOP_MID, 0, 5);


    // Update Nucleus
    if (!nucleus_obj) {
        nucleus_obj = lv_obj_create(animation_container);
        lv_obj_remove_style_all(nucleus_obj);
        lv_obj_add_style(nucleus_obj, &style_nucleus, 0);
        lv_obj_set_size(nucleus_obj, NUCLEUS_RADIUS * 2, NUCLEUS_RADIUS * 2);
    }
    lv_obj_set_pos(nucleus_obj, center_x - NUCLEUS_RADIUS, center_y - NUCLEUS_RADIUS); // Use set_pos
    lv_obj_clear_flag(nucleus_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(nucleus_obj, LV_OPA_COVER, 0);

    // Fade-in animation setup
    lv_anim_t a_fade_in;
    lv_anim_init(&a_fade_in);
    lv_anim_set_time(&a_fade_in, FADE_TIME_MS);
    lv_anim_set_delay(&a_fade_in, 50);
    lv_anim_set_exec_cb(&a_fade_in, fade_anim_opa_exec_cb);
    lv_anim_set_values(&a_fade_in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_ready_cb(&a_fade_in, NULL);
    lv_anim_set_deleted_cb(&a_fade_in, NULL);

    // Update Orbits
    for (int shell = 0; shell < MAX_SHELLS; shell++) {
        if (element->electrons_per_shell[shell] > 0) {
            lv_coord_t radius = element_radii[shell];
            if (!orbit_objs[shell]) { continue; } // Should exist
            lv_obj_set_size(orbit_objs[shell], radius * 2, radius * 2);
            lv_obj_set_pos(orbit_objs[shell], center_x - radius, center_y - radius); // Use set_pos

            lv_obj_set_style_opa(orbit_objs[shell], LV_OPA_TRANSP, 0);
            lv_obj_clear_flag(orbit_objs[shell], LV_OBJ_FLAG_HIDDEN);
            lv_anim_del(orbit_objs[shell], fade_anim_opa_exec_cb);
            a_fade_in.var = orbit_objs[shell];
            lv_anim_start(&a_fade_in);
        } else {
             if(orbit_objs[shell]) lv_obj_add_flag(orbit_objs[shell], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Update Electrons and Animations
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
             if (!electron_objs[electron_count]) { continue; } // Should exist
             lv_obj_set_user_data(electron_objs[electron_count], (void*)(lv_uintptr_t)radius);

             // Start fade in
             lv_obj_set_style_opa(electron_objs[electron_count], LV_OPA_TRANSP, 0);
             lv_obj_clear_flag(electron_objs[electron_count], LV_OBJ_FLAG_HIDDEN);
             lv_anim_del(electron_objs[electron_count], fade_anim_opa_exec_cb);
             a_fade_in.var = electron_objs[electron_count];
             lv_anim_start(&a_fade_in);

            // Start position animation
            int32_t start_angle = (int32_t)(start_angle_offset + i * angle_step) % 360;
            int32_t end_angle = start_angle + (anim_direction * 359);
            float start_angle_rad = (float)(start_angle % 360) * M_PI / 180.0f;
            lv_coord_t start_x = center_x + (lv_coord_t)(radius * cosf(start_angle_rad)) - ELECTRON_RADIUS;
            lv_coord_t start_y = center_y + (lv_coord_t)(radius * sinf(start_angle_rad)) - ELECTRON_RADIUS;
            lv_obj_set_pos(electron_objs[electron_count], start_x, start_y);

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
     // Hide remaining unused electron objects
     for (int i = electron_count; i < MAX_ELECTRONS; i++) {
        if(electron_objs[i]) lv_obj_add_flag(electron_objs[i], LV_OBJ_FLAG_HIDDEN);
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
 * @brief Initialize UI: Split view, adjusted animation position.
 */
void my_ui_init(void) {
    lv_obj_t * screen = lv_scr_act();
    lv_obj_clean(screen);

    // --- Initialize Styles (Tech Theme) ---
    lv_style_init(&style_canvas_bg); lv_style_set_bg_opa(&style_canvas_bg, LV_OPA_COVER); lv_style_set_bg_color(&style_canvas_bg, lv_color_hex(0x101820));
    lv_style_init(&style_nucleus); lv_style_set_radius(&style_nucleus, LV_RADIUS_CIRCLE); lv_style_set_bg_opa(&style_nucleus, LV_OPA_COVER); lv_style_set_bg_color(&style_nucleus, lv_palette_lighten(LV_PALETTE_BLUE, 1)); lv_style_set_border_width(&style_nucleus, 0);
    lv_style_init(&style_electron); lv_style_set_radius(&style_electron, LV_RADIUS_CIRCLE); lv_style_set_bg_opa(&style_electron, LV_OPA_COVER); lv_style_set_bg_color(&style_electron, lv_palette_main(LV_PALETTE_CYAN)); lv_style_set_border_width(&style_electron, 0); lv_style_set_shadow_width(&style_electron, 6); lv_style_set_shadow_spread(&style_electron, 1); lv_style_set_shadow_color(&style_electron, lv_palette_lighten(LV_PALETTE_CYAN, 1)); lv_style_set_shadow_opa(&style_electron, LV_OPA_80);
    lv_style_init(&style_shell_line); lv_style_set_arc_color(&style_shell_line, lv_color_hex(0x506070)); lv_style_set_arc_width(&style_shell_line, ORBIT_LINE_WIDTH); lv_style_set_bg_opa(&style_shell_line, LV_OPA_TRANSP); lv_style_set_arc_rounded(&style_shell_line, true);
    lv_style_transition_dsc_init(&btn_trans, btn_trans_props, lv_anim_path_ease_out, 100, 0, NULL);
    lv_style_init(&style_button); lv_style_set_radius(&style_button, 6); lv_style_set_bg_opa(&style_button, LV_OPA_COVER); lv_style_set_bg_color(&style_button, lv_color_hex(0x2A3B4D)); lv_style_set_bg_grad_color(&style_button, lv_color_hex(0x4A5B6D)); lv_style_set_bg_grad_dir(&style_button, LV_GRAD_DIR_VER); lv_style_set_border_width(&style_button, 1); lv_style_set_border_color(&style_button, lv_color_hex(0x6A7B8D)); lv_style_set_text_color(&style_button, lv_color_white()); lv_style_set_pad_all(&style_button, 7); lv_style_set_transition(&style_button, &btn_trans);
    lv_style_init(&style_button_pressed); lv_style_set_transform_width(&style_button_pressed, -2); lv_style_set_transform_height(&style_button_pressed, -2); lv_style_set_bg_color(&style_button_pressed, lv_color_hex(0x1A2B3D)); lv_style_set_bg_grad_color(&style_button_pressed, lv_color_hex(0x3A4B5D));
    lv_style_init(&style_info_panel); lv_style_set_bg_color(&style_info_panel, lv_color_hex(0x182028)); lv_style_set_bg_opa(&style_info_panel, LV_OPA_COVER); lv_style_set_pad_all(&style_info_panel, 10); lv_style_set_radius(&style_info_panel, 5);
    lv_style_init(&style_info_label_title); lv_style_set_text_color(&style_info_label_title, lv_palette_main(LV_PALETTE_CYAN)); lv_style_set_text_font(&style_info_label_title, &lv_font_montserrat_16);
    lv_style_init(&style_info_label_static); lv_style_set_text_color(&style_info_label_static, lv_palette_lighten(LV_PALETTE_GREY, 1)); lv_style_set_text_font(&style_info_label_static, &lv_font_montserrat_12);
    lv_style_init(&style_info_label_value); lv_style_set_text_color(&style_info_label_value, lv_color_white()); lv_style_set_text_font(&style_info_label_value, &lv_font_montserrat_12);


    // --- Set Main Screen Background ---
    lv_obj_add_style(screen, &style_canvas_bg, 0);

    // --- Create Top Container ---
    main_container_top = lv_obj_create(screen);
    lv_obj_remove_style_all(main_container_top);
    lv_obj_set_size(main_container_top, lv_pct(100), lv_pct(62)); // Top ~62% height
    lv_obj_align(main_container_top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(main_container_top, LV_FLEX_FLOW_ROW);

    // --- Create Animation Container (Left) ---
    animation_container = lv_obj_create(main_container_top);
    lv_obj_remove_style_all(animation_container);
    lv_obj_set_style_bg_opa(animation_container, LV_OPA_TRANSP, 0);
    lv_obj_set_size(animation_container, lv_pct(65), lv_pct(100)); // 65% width

    // --- Create Info Panel (Right) ---
    info_panel = lv_obj_create(main_container_top);
    lv_obj_remove_style_all(info_panel);
    lv_obj_add_style(info_panel, &style_info_panel, 0);
    lv_obj_set_size(info_panel, lv_pct(35), lv_pct(100)); // 35% width
    lv_obj_set_flex_flow(info_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(info_panel, 5, 0);

    // --- Create Button Container (Bottom) ---
    button_container = lv_obj_create(screen);
    lv_obj_remove_style_all(button_container);
    lv_obj_set_width(button_container, lv_pct(96));
    lv_obj_set_height(button_container, lv_pct(35)); // Bottom ~35% height
    lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_container, 5, 0);


    // --- Force Layout & Calculate Animation Center ---
    // *** This is important to get dimensions BEFORE calculating center ***
    lv_obj_update_layout(screen); // Update layout for the whole screen
    lv_coord_t anim_w = lv_obj_get_content_width(animation_container);
    lv_coord_t anim_h = lv_obj_get_content_height(animation_container);
    center_x = anim_w / 2;
    // *** ADJUSTED: Center vertically within animation container ***
    center_y = anim_h / 2; // Use 50% of the container height
    LV_LOG_USER("Animation container actual size: %dx%d, Center: X=%d, Y=%d", (int)anim_w, (int)anim_h, (int)center_x, (int)center_y);


    // --- Create Element Name Label (Inside Animation Container) ---
    element_label = lv_label_create(animation_container);
    lv_obj_set_width(element_label, lv_pct(90));
    lv_obj_set_style_text_align(element_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(element_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_font(element_label, &lv_font_montserrat_16, 0);
    lv_obj_align(element_label, LV_ALIGN_TOP_MID, 0, 5); // Align near top of animation container


    // --- Create Info Panel Labels ---
    // (Code for creating labels is unchanged from previous version)
    label_info_title = lv_label_create(info_panel); lv_obj_add_style(label_info_title, &style_info_label_title, 0); lv_label_set_text(label_info_title, "Element Details"); lv_obj_set_width(label_info_title, lv_pct(100)); lv_obj_set_style_text_align(label_info_title, LV_TEXT_ALIGN_CENTER, 0);
    label_atomic_num_lbl = lv_label_create(info_panel); lv_obj_add_style(label_atomic_num_lbl, &style_info_label_static, 0); lv_label_set_text(label_atomic_num_lbl, "Atomic Num:");
    label_atomic_num_val = lv_label_create(info_panel); lv_obj_add_style(label_atomic_num_val, &style_info_label_value, 0); lv_label_set_text(label_atomic_num_val, "-");
    label_atomic_mass_lbl = lv_label_create(info_panel); lv_obj_add_style(label_atomic_mass_lbl, &style_info_label_static, 0); lv_label_set_text(label_atomic_mass_lbl, "Atomic Mass:");
    label_atomic_mass_val = lv_label_create(info_panel); lv_obj_add_style(label_atomic_mass_val, &style_info_label_value, 0); lv_label_set_text(label_atomic_mass_val, "-");
    label_config_lbl = lv_label_create(info_panel); lv_obj_add_style(label_config_lbl, &style_info_label_static, 0); lv_label_set_text(label_config_lbl, "Config:");
    label_config_val = lv_label_create(info_panel); lv_obj_add_style(label_config_val, &style_info_label_value, 0); lv_label_set_long_mode(label_config_val, LV_LABEL_LONG_WRAP); lv_obj_set_width(label_config_val, lv_pct(100)); lv_label_set_text(label_config_val, "-");
    label_category_lbl = lv_label_create(info_panel); lv_obj_add_style(label_category_lbl, &style_info_label_static, 0); lv_label_set_text(label_category_lbl, "Category:");
    label_category_val = lv_label_create(info_panel); lv_obj_add_style(label_category_val, &style_info_label_value, 0); lv_label_set_long_mode(label_category_val, LV_LABEL_LONG_WRAP); lv_obj_set_width(label_category_val, lv_pct(100)); lv_label_set_text(label_category_val, "-");


    // Create buttons for the first 44 elements
    // (Button creation loop is unchanged)
    for (uint8_t i = 0; i < MAX_ELEMENTS; i++) {
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


    // --- Pre-allocate animation objects ---
    // (Pre-allocation code is unchanged, parent is animation_container)
    nucleus_obj = NULL;
    for(int i=0; i<MAX_ELECTRONS; i++) {
        electron_objs[i] = lv_obj_create(animation_container);
        lv_obj_remove_style_all(electron_objs[i]);
        lv_obj_add_style(electron_objs[i], &style_electron, 0);
        lv_obj_set_size(electron_objs[i], ELECTRON_RADIUS * 2, ELECTRON_RADIUS * 2);
        lv_obj_add_flag(electron_objs[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
    }
     for(int i=0; i<MAX_SHELLS; i++) {
        orbit_objs[i] = lv_arc_create(animation_container);
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

    LV_LOG_USER("Custom UI Initialized: 44 Elements, Info Panel, V-Centered Animation.");
}