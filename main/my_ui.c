// my_ui.c
#include "my_ui.h"
#include "lvgl.h"
#include <math.h>   // For sinf, cosf, M_PI
#include <stdlib.h> // For rand, abs

// --- Configuration for 480x480 Screen ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

#define MAX_ELECTRONS 10 // Maximum electrons for Neon
#define MAX_SHELLS 2     // Maximum shells needed for the first 10 elements

// Adjusted sizes for better look on 480x480
// Make radii slightly smaller if buttons need more space without overlap
#define NUCLEUS_RADIUS 18
#define ELECTRON_RADIUS 7
#define SHELL1_RADIUS 80
#define SHELL2_RADIUS 140  // Max radius is 140 + 7 = 147. Center Y is 240. Plenty of space.
#define ORBIT_LINE_WIDTH 2

#define ANIMATION_TIME_MS 6000 // Animation time

// --- Data Structure for Elements (No changes) ---
typedef struct {
    const char *symbol;
    const char *name;
    uint8_t total_electrons;
    uint8_t electrons_per_shell[MAX_SHELLS]; // [Shell 1, Shell 2]
} element_info_t;

// Data for the first 10 elements (No changes)
static const element_info_t elements[10] = {
    {"H",  "Hydrogen", 1, {1, 0}},
    {"He", "Helium",   2, {2, 0}},
    {"Li", "Lithium",  3, {2, 1}},
    {"Be", "Beryllium",4, {2, 2}},
    {"B",  "Boron",    5, {2, 3}},
    {"C",  "Carbon",   6, {2, 4}},
    {"N",  "Nitrogen", 7, {2, 5}},
    {"O",  "Oxygen",   8, {2, 6}},
    {"F",  "Fluorine", 9, {2, 7}},
    {"Ne", "Neon",    10, {2, 8}}
};

// --- Static UI Variables ---
// Rename main_container to animation_canvas as it covers the whole screen now
static lv_obj_t * animation_canvas;     // Full screen container for animation + overlay elements
static lv_obj_t * button_container;     // Container for element buttons (Overlay Bottom)
static lv_obj_t * element_label;        // Label to display element name/symbol (Overlay Top)
static lv_obj_t * nucleus_obj;          // Object representing the nucleus
static lv_obj_t * electron_objs[MAX_ELECTRONS]; // Array to hold electron objects
static lv_obj_t * orbit_objs[MAX_SHELLS];   // Array to hold orbit line objects
static lv_anim_t electron_anims[MAX_ELECTRONS]; // Array to hold electron animations
static uint8_t current_element_index = 0; // Index of the currently displayed element
static lv_coord_t center_x; // Center of the full screen
static lv_coord_t center_y; // Center of the full screen

// --- Forward Declarations ---
static void create_element_display(uint8_t element_index);
static void clear_element_display(void);
static void electron_anim_exec_cb(void * var, int32_t v);
static void element_select_event_cb(lv_event_t * e);

// --- Style Definitions ---
static lv_style_t style_nucleus;
static lv_style_t style_electron;
static lv_style_t style_shell_line; // Style for drawing orbits
static lv_style_t style_button;     // Custom button style

// --- Function Implementations ---

/**
 * @brief Callback function for electron animation. Calculates and sets position.
 */
static void electron_anim_exec_cb(void * var, int32_t v) {
    // (No changes needed in this function)
    lv_obj_t * obj = (lv_obj_t *)var;
    lv_coord_t radius = (lv_coord_t)(lv_uintptr_t)lv_obj_get_user_data(obj);
    float angle_rad = (float)(v % 360) * M_PI / 180.0f;
    // Use the globally calculated screen center_x, center_y
    lv_coord_t x = center_x + (lv_coord_t)(radius * cosf(angle_rad)) - ELECTRON_RADIUS;
    lv_coord_t y = center_y + (lv_coord_t)(radius * sinf(angle_rad)) - ELECTRON_RADIUS;
    lv_obj_set_pos(obj, x, y);
}

/**
 * @brief Clears previous element's electrons, orbits, and animations.
 */
static void clear_element_display(void) {
    // (No changes needed in this function)
    for (int i = 0; i < MAX_ELECTRONS; i++) {
        if (electron_objs[i]) {
            lv_anim_del(electron_objs[i], electron_anim_exec_cb);
            lv_obj_del(electron_objs[i]);
            electron_objs[i] = NULL;
        }
    }
    for (int i = 0; i < MAX_SHELLS; i++) {
        if (orbit_objs[i]) {
            lv_obj_del(orbit_objs[i]);
            orbit_objs[i] = NULL;
        }
    }
    // Keep the nucleus object, just realign if needed (though should be ok)
}

/**
 * @brief Creates the visual representation (nucleus, orbits, electrons, animations) centered on screen.
 * @param element_index Index in the `elements` array.
 */
static void create_element_display(uint8_t element_index) {
    if (element_index >= sizeof(elements) / sizeof(elements[0])) {
        return; // Invalid index
    }

    clear_element_display(); // Remove the old display first

    const element_info_t *element = &elements[element_index];
    current_element_index = element_index;

    // Update label text (Label position is set in my_ui_init)
    lv_label_set_text_fmt(element_label, "%s - %s", element->symbol, element->name);

    // Ensure Nucleus exists and is centered on the canvas (screen)
    if (!nucleus_obj) {
        nucleus_obj = lv_obj_create(animation_canvas); // Parent is the full screen canvas
        lv_obj_remove_style_all(nucleus_obj);
        lv_obj_add_style(nucleus_obj, &style_nucleus, 0);
        lv_obj_set_size(nucleus_obj, NUCLEUS_RADIUS * 2, NUCLEUS_RADIUS * 2);
    }
    // Align nucleus to the globally calculated screen center
    lv_obj_align(nucleus_obj, LV_ALIGN_CENTER, 0, 0);


    // --- Create Orbits (Centered on Screen) ---
    // Shell 1 Orbit Line
    if (element->electrons_per_shell[0] > 0) {
        orbit_objs[0] = lv_arc_create(animation_canvas); // Parent is the full screen canvas
        lv_obj_remove_style(orbit_objs[0], NULL, LV_PART_KNOB);
        lv_obj_clear_flag(orbit_objs[0], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_style(orbit_objs[0], &style_shell_line, LV_PART_MAIN);
        lv_obj_add_style(orbit_objs[0], &style_shell_line, LV_PART_INDICATOR);
        lv_obj_set_size(orbit_objs[0], SHELL1_RADIUS * 2, SHELL1_RADIUS * 2);
        lv_arc_set_bg_angles(orbit_objs[0], 0, 360);
        lv_arc_set_angles(orbit_objs[0], 0, 360);
        lv_obj_align(orbit_objs[0], LV_ALIGN_CENTER, 0, 0); // Align to screen center
    }
     // Shell 2 Orbit Line
    if (element->electrons_per_shell[1] > 0) {
        orbit_objs[1] = lv_arc_create(animation_canvas); // Parent is the full screen canvas
        lv_obj_remove_style(orbit_objs[1], NULL, LV_PART_KNOB);
        lv_obj_clear_flag(orbit_objs[1], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_style(orbit_objs[1], &style_shell_line, LV_PART_MAIN);
        lv_obj_add_style(orbit_objs[1], &style_shell_line, LV_PART_INDICATOR);
        lv_obj_set_size(orbit_objs[1], SHELL2_RADIUS * 2, SHELL2_RADIUS * 2);
        lv_arc_set_bg_angles(orbit_objs[1], 0, 360);
        lv_arc_set_angles(orbit_objs[1], 0, 360);
        lv_obj_align(orbit_objs[1], LV_ALIGN_CENTER, 0, 0); // Align to screen center
    }

    // --- Create Electrons and Animations (relative to screen center) ---
    uint8_t electron_count = 0;
    float start_angle_offset = 0;

    // Shell 1 Electrons
    uint8_t shell1_electrons = element->electrons_per_shell[0];
    float angle_step1 = (shell1_electrons > 0) ? 360.0f / shell1_electrons : 0;
    for (int i = 0; i < shell1_electrons && electron_count < MAX_ELECTRONS; i++) {
        electron_objs[electron_count] = lv_obj_create(animation_canvas); // Parent is the canvas
        lv_obj_remove_style_all(electron_objs[electron_count]);
        lv_obj_add_style(electron_objs[electron_count], &style_electron, 0);
        lv_obj_set_size(electron_objs[electron_count], ELECTRON_RADIUS * 2, ELECTRON_RADIUS * 2);
        lv_obj_set_user_data(electron_objs[electron_count], (void*)(lv_uintptr_t)SHELL1_RADIUS);

        int32_t start_angle = (int32_t)(start_angle_offset + i * angle_step1) % 360;
        lv_anim_init(&electron_anims[electron_count]);
        lv_anim_set_var(&electron_anims[electron_count], electron_objs[electron_count]);
        lv_anim_set_exec_cb(&electron_anims[electron_count], electron_anim_exec_cb);
        lv_anim_set_values(&electron_anims[electron_count], start_angle, start_angle + 359);
        lv_anim_set_time(&electron_anims[electron_count], ANIMATION_TIME_MS + (rand() % 500 - 250));
        lv_anim_set_playback_time(&electron_anims[electron_count], 0);
        lv_anim_set_repeat_count(&electron_anims[electron_count], LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&electron_anims[electron_count]);
        electron_count++;
    }

    // Shell 2 Electrons
    uint8_t shell2_electrons = element->electrons_per_shell[1];
    float angle_step2 = (shell2_electrons > 0) ? 360.0f / shell2_electrons : 0;
    start_angle_offset = 45;
    for (int i = 0; i < shell2_electrons && electron_count < MAX_ELECTRONS; i++) {
         electron_objs[electron_count] = lv_obj_create(animation_canvas); // Parent is the canvas
        lv_obj_remove_style_all(electron_objs[electron_count]);
        lv_obj_add_style(electron_objs[electron_count], &style_electron, 0);
        lv_obj_set_size(electron_objs[electron_count], ELECTRON_RADIUS * 2, ELECTRON_RADIUS * 2);
        lv_obj_set_user_data(electron_objs[electron_count], (void*)(lv_uintptr_t)SHELL2_RADIUS);

        int32_t start_angle = (int32_t)(start_angle_offset + i * angle_step2) % 360;
        lv_anim_init(&electron_anims[electron_count]);
        lv_anim_set_var(&electron_anims[electron_count], electron_objs[electron_count]);
        lv_anim_set_exec_cb(&electron_anims[electron_count], electron_anim_exec_cb);
        lv_anim_set_values(&electron_anims[electron_count], start_angle, start_angle + 359);
        lv_anim_set_time(&electron_anims[electron_count], ANIMATION_TIME_MS + (rand() % 500 - 250));
        lv_anim_set_playback_time(&electron_anims[electron_count], 0);
        lv_anim_set_repeat_count(&electron_anims[electron_count], LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&electron_anims[electron_count]);
        electron_count++;
    }
}


/**
 * @brief Event callback for element selection buttons.
 */
static void element_select_event_cb(lv_event_t * e) {
    // (No changes needed in this function)
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        uint8_t index = (uint8_t)(lv_uintptr_t)lv_obj_get_user_data(btn);
        if (index != current_element_index) {
           LV_LOG_USER("Button clicked for element index: %d", index);
           create_element_display(index);
        }
    }
}

/**
 * @brief Initialize the custom UI elements: Animation centered on full screen, overlays for label/buttons.
 */
void my_ui_init(void) {
    // Get the active screen
    lv_obj_t * screen = lv_scr_act();
    lv_obj_clean(screen); // Clear screen if reusing

    // Calculate the absolute center of the screen
    center_x = SCREEN_WIDTH / 2;
    center_y = SCREEN_HEIGHT / 2;

    // --- Initialize Styles (Same as before) ---
    // Nucleus Style
    lv_style_init(&style_nucleus);
    lv_style_set_radius(&style_nucleus, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_nucleus, LV_OPA_COVER);
    lv_style_set_bg_color(&style_nucleus, lv_palette_main(LV_PALETTE_DEEP_ORANGE));
    lv_style_set_border_width(&style_nucleus, 0);

    // Electron Style
    lv_style_init(&style_electron);
    lv_style_set_radius(&style_electron, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa(&style_electron, LV_OPA_COVER);
    lv_style_set_bg_color(&style_electron, lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 1));
    lv_style_set_border_width(&style_electron, 1);
    lv_style_set_border_color(&style_electron, lv_palette_main(LV_PALETTE_GREY));


    // Orbit Line Style
    lv_style_init(&style_shell_line);
    lv_style_set_arc_color(&style_shell_line, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_arc_width(&style_shell_line, ORBIT_LINE_WIDTH);
    lv_style_set_bg_opa(&style_shell_line, LV_OPA_TRANSP); // Make bg part transparent
    lv_style_set_arc_rounded(&style_shell_line, false); // Ensure full circle line

    // Button Style
    lv_style_init(&style_button);
    lv_style_set_radius(&style_button, 8);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_bg_color(&style_button, lv_palette_main(LV_PALETTE_BLUE_GREY));
    lv_style_set_border_width(&style_button, 1);
    lv_style_set_border_color(&style_button, lv_palette_lighten(LV_PALETTE_BLUE_GREY, 2));
    lv_style_set_text_color(&style_button, lv_color_white());
    lv_style_set_pad_ver(&style_button, 10);
    lv_style_set_pad_hor(&style_button, 5);


    // --- Create Full Screen Canvas for Animation ---
    animation_canvas = lv_obj_create(screen);
    lv_obj_remove_style_all(animation_canvas); // Remove border/padding
    lv_obj_set_size(animation_canvas, SCREEN_WIDTH, SCREEN_HEIGHT); // Cover entire screen
    lv_obj_align(animation_canvas, LV_ALIGN_CENTER, 0, 0);
    // Set background color directly on the canvas
    lv_obj_set_style_bg_color(animation_canvas, lv_palette_darken(LV_PALETTE_GREY, 4), 0); // Dark grey background
    lv_obj_set_style_bg_opa(animation_canvas, LV_OPA_COVER, 0);


    // --- Create Element Name Label (Overlay on Top) ---
    element_label = lv_label_create(animation_canvas); // Parent is the canvas
    lv_obj_set_width(element_label, lv_pct(90)); // Use 90% width
    lv_obj_set_style_text_align(element_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(element_label, lv_color_white(), 0);
    // Align to Top-Mid of the parent (canvas) with some padding
    lv_obj_align(element_label, LV_ALIGN_TOP_MID, 0, 15); // 15px padding from top


    // --- Create Button Container (Overlay on Bottom) ---
    button_container = lv_obj_create(animation_canvas); // Parent is the canvas
    lv_obj_remove_style_all(button_container); // Remove its own background/border/padding
    lv_obj_set_width(button_container, lv_pct(95)); // Use 95% of screen width
    lv_obj_set_height(button_container, LV_SIZE_CONTENT); // Height determined by buttons
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW); // Arrange buttons horizontally
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_container, 8, 0); // Gap between buttons
    // Align container to the Bottom-Mid of the parent (canvas) with padding
    lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, -15); // -15px padding from bottom

    // Create buttons for each element inside the container
    for (uint8_t i = 0; i < sizeof(elements) / sizeof(elements[0]); i++) {
        lv_obj_t * btn = lv_btn_create(button_container);
        lv_obj_add_style(btn, &style_button, 0); // Apply custom button style
        lv_obj_set_user_data(btn, (void*)(lv_uintptr_t)i);
        lv_obj_add_event_cb(btn, element_select_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, elements[i].symbol);
        lv_obj_center(lbl);
    }


    // --- Final Steps ---
    // Reset object pointers (important before first create)
    for (int i = 0; i < MAX_ELECTRONS; i++) { electron_objs[i] = NULL; }
    for (int i = 0; i < MAX_SHELLS; i++) { orbit_objs[i] = NULL; }
    nucleus_obj = NULL; // Ensure nucleus is created fresh


    // --- Display the first element initially ---
    // Center X/Y are now correctly calculated for the full screen
    create_element_display(0); // Start with Hydrogen

    LV_LOG_USER("Custom UI Initialized: Animation screen-centered.");
}