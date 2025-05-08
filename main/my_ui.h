// my_ui.h
#ifndef MY_UI_H
#define MY_UI_H

#include "lvgl.h"
#include <stdbool.h> // For bool type

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes and renders the UI based on a JSON configuration string.
 *
 * This function will clear any existing UI elements on the active screen
 * and then parse the provided JSON string to create new LVGL widgets.
 *
 * @param json_string A null-terminated string containing the UI configuration in JSON format.
 * @return true if parsing and rendering was successful, false otherwise.
 */
bool my_ui_render_from_json(const char *json_string);

/**
 * @brief Initializes predefined LVGL styles that can be referenced by JSON.
 * Call this once after LVGL is initialized. This version prioritizes generic
 * English fonts.
 */
void my_ui_styles_init(void);


// --- Predefined Style References (add more as needed) ---
// These extern declarations allow my_ui.c to define them and other files to use them if necessary,
// though primarily they are used internally by the JSON renderer via style_ref.

extern lv_style_t style_font_large_title;
extern lv_style_t style_font_normal_text;
extern lv_style_t style_font_small_text;
extern lv_style_t style_button_primary;
extern lv_style_t style_button_danger;
extern lv_style_t style_card;


#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* MY_UI_H */