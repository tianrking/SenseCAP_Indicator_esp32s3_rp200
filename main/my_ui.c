// my_ui.c
#include "my_ui.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h> // For atoi, strtol

static const char *TAG = "my_ui_json";

// --- Style Definitions ---
lv_style_t style_font_large_title;
lv_style_t style_font_normal_text;
lv_style_t style_font_small_text;
lv_style_t style_button_primary;
lv_style_t style_button_danger;
lv_style_t style_card;

// Forward declarations for helper functions
static lv_obj_t* create_widget_from_json(lv_obj_t *parent, cJSON *widget_config);
static void apply_common_styles(lv_obj_t *obj, cJSON *json_config);
static void apply_layout_to_container(lv_obj_t *container, cJSON *layout_config);
static lv_color_t parse_color_string(const char *hex_color_str);
static void apply_style_ref(lv_obj_t *obj, const char *style_ref_str);

void my_ui_styles_init(void) {
    // Font Usage Notes:
    // This setup prioritizes generic English fonts. LV_FONT_DEFAULT is the most generic.
    // For varied text sizes, specific Montserrat font sizes are attempted.
    // These Montserrat fonts (e.g., CONFIG_LV_FONT_MONTSERRAT_20) need to be
    // enabled in your LVGL configuration (idf.py menuconfig -> Component config -> LVGL -> Font usage).
    // If not enabled, the code gracefully falls back to LV_FONT_DEFAULT.

    lv_style_init(&style_font_large_title);
    #if CONFIG_LV_FONT_MONTSERRAT_20
    lv_style_set_text_font(&style_font_large_title, &lv_font_montserrat_20);
    ESP_LOGD(TAG, "Using Montserrat 20 for large titles.");
    #else
    lv_style_set_text_font(&style_font_large_title, LV_FONT_DEFAULT);
    ESP_LOGD(TAG, "Montserrat 20 not enabled, using LV_FONT_DEFAULT for large titles.");
    #endif

    lv_style_init(&style_font_normal_text);
    #if CONFIG_LV_FONT_MONTSERRAT_14
    lv_style_set_text_font(&style_font_normal_text, &lv_font_montserrat_14);
    ESP_LOGD(TAG, "Using Montserrat 14 for normal text.");
    #else
    lv_style_set_text_font(&style_font_normal_text, LV_FONT_DEFAULT);
    ESP_LOGD(TAG, "Montserrat 14 not enabled, using LV_FONT_DEFAULT for normal text.");
    #endif

    lv_style_init(&style_font_small_text);
    #if CONFIG_LV_FONT_MONTSERRAT_10
    lv_style_set_text_font(&style_font_small_text, &lv_font_montserrat_10);
    ESP_LOGD(TAG, "Using Montserrat 10 for small text.");
    #else
    lv_style_set_text_font(&style_font_small_text, LV_FONT_DEFAULT);
    ESP_LOGD(TAG, "Montserrat 10 not enabled, using LV_FONT_DEFAULT for small text.");
    #endif

    lv_style_init(&style_button_primary);
    lv_style_set_bg_color(&style_button_primary, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_text_color(&style_button_primary, lv_color_white());
    lv_style_set_pad_all(&style_button_primary, 10);
    lv_style_set_radius(&style_button_primary, 5);

    lv_style_init(&style_button_danger);
    lv_style_set_bg_color(&style_button_danger, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_color(&style_button_danger, lv_color_white());
    lv_style_set_pad_all(&style_button_danger, 10);
    lv_style_set_radius(&style_button_danger, 5);

    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(0xf9f9f9));
    lv_style_set_border_color(&style_card, lv_color_hex(0xdddddd));
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 6);
    lv_style_set_shadow_width(&style_card, 5);
    lv_style_set_shadow_opa(&style_card, LV_OPA_10);
    lv_style_set_shadow_ofs_y(&style_card, 2);
    lv_style_set_pad_all(&style_card, 10);
}


static lv_color_t parse_color_string(const char *hex_color_str) {
    if (!hex_color_str || hex_color_str[0] != '#') {
        ESP_LOGW(TAG, "Invalid color string: %s, defaulting to black", hex_color_str ? hex_color_str : "NULL");
        return lv_color_black();
    }
    unsigned long color_val = strtoul(hex_color_str + 1, NULL, 16);
    return lv_color_hex(color_val);
}

static void apply_style_ref(lv_obj_t *obj, const char *style_ref_str) {
    if (!obj || !style_ref_str) return;

    if (strcmp(style_ref_str, "theme.font.large_title") == 0 || strcmp(style_ref_str, "theme-font-large_title") == 0) {
        lv_obj_add_style(obj, &style_font_large_title, 0);
    } else if (strcmp(style_ref_str, "theme.font.normal_text") == 0 || strcmp(style_ref_str, "theme-font-normal_text") == 0) {
        lv_obj_add_style(obj, &style_font_normal_text, 0);
    } else if (strcmp(style_ref_str, "theme.font.small_text") == 0 || strcmp(style_ref_str, "theme-font-small_text") == 0) {
        lv_obj_add_style(obj, &style_font_small_text, 0);
    } else if (strcmp(style_ref_str, "theme.button.primary") == 0 || strcmp(style_ref_str, "theme-button-primary") == 0) {
        lv_obj_add_style(obj, &style_button_primary, 0);
    } else if (strcmp(style_ref_str, "theme.button.danger") == 0 || strcmp(style_ref_str, "theme-button-danger") == 0) {
        lv_obj_add_style(obj, &style_button_danger, 0);
    } else if (strcmp(style_ref_str, "theme.card") == 0 || strcmp(style_ref_str, "theme-card") == 0) {
        lv_obj_add_style(obj, &style_card, 0);
    } else {
        ESP_LOGW(TAG, "Unknown style_ref: %s", style_ref_str);
    }
}


static void apply_common_styles(lv_obj_t *obj, cJSON *json_config) {
    if (!obj || !json_config) return;

    cJSON *item;
    // Apply style_ref first, so specific JSON properties can override parts of it.
    cJSON *style_ref_item = cJSON_GetObjectItemCaseSensitive(json_config, "style_ref");
    if (cJSON_IsString(style_ref_item) && (style_ref_item->valuestring != NULL)) {
        apply_style_ref(obj, style_ref_item->valuestring);
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "id");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        ESP_LOGD(TAG, "Widget ID (JSON): %s, LVGL obj: %p", item->valuestring, obj);
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "background_color");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        lv_obj_set_style_bg_color(obj, parse_color_string(item->valuestring), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "text_color");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        lv_obj_set_style_text_color(obj, parse_color_string(item->valuestring), LV_PART_MAIN);
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "width");
    if (cJSON_IsNumber(item)) {
        lv_obj_set_width(obj, (lv_coord_t)item->valueint);
    } else if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        if (strstr(item->valuestring, "%")) {
            lv_obj_set_width(obj, LV_PCT(atoi(item->valuestring)));
        } else {
            lv_obj_set_width(obj, (lv_coord_t)atoi(item->valuestring));
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "height");
    if (cJSON_IsNumber(item)) {
        lv_obj_set_height(obj, (lv_coord_t)item->valueint);
    } else if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        if (strstr(item->valuestring, "%")) {
            lv_obj_set_height(obj, LV_PCT(atoi(item->valuestring)));
        } else {
            lv_obj_set_height(obj, (lv_coord_t)atoi(item->valuestring));
        }
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "padding");
    if (cJSON_IsNumber(item)) {
        lv_obj_set_style_pad_all(obj, (lv_coord_t)item->valueint, LV_PART_MAIN);
    } else if (cJSON_IsString(item)){
        lv_obj_set_style_pad_all(obj, (lv_coord_t)atoi(item->valuestring), LV_PART_MAIN);
    }

    item = cJSON_GetObjectItemCaseSensitive(json_config, "border_radius");
    if (cJSON_IsNumber(item)) {
        lv_obj_set_style_radius(obj, (lv_coord_t)item->valueint, LV_PART_MAIN);
    }
}

static void apply_layout_to_container(lv_obj_t *container, cJSON *layout_config) {
    if (!container || !layout_config) return;

    lv_obj_set_layout(container, LV_LAYOUT_FLEX);

    cJSON *item;
    lv_flex_flow_t flow = LV_FLEX_FLOW_COLUMN;
    item = cJSON_GetObjectItemCaseSensitive(layout_config, "type");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        if (strcmp(item->valuestring, "row") == 0) {
            flow = LV_FLEX_FLOW_ROW;
        }
    }
    lv_obj_set_flex_flow(container, flow);

    item = cJSON_GetObjectItemCaseSensitive(layout_config, "gap");
    if (cJSON_IsNumber(item)) {
        lv_coord_t gap_val = (lv_coord_t)item->valueint;
        lv_obj_set_style_pad_row(container, gap_val, LV_PART_MAIN);
        lv_obj_set_style_pad_column(container, gap_val, LV_PART_MAIN);
    }
    
    item = cJSON_GetObjectItemCaseSensitive(layout_config, "padding");
    if (cJSON_IsNumber(item)) {
        lv_obj_set_style_pad_all(container, (lv_coord_t)item->valueint, LV_PART_MAIN);
    } else if (cJSON_IsString(item)){
        lv_obj_set_style_pad_all(container, (lv_coord_t)atoi(item->valuestring), LV_PART_MAIN);
    }

    lv_flex_align_t main_place = lv_obj_get_style_flex_main_place(container, LV_PART_MAIN);
    lv_flex_align_t cross_place = lv_obj_get_style_flex_cross_place(container, LV_PART_MAIN);
    lv_flex_align_t track_cross_place = lv_obj_get_style_flex_track_place(container, LV_PART_MAIN);

    if (main_place == 0 && cross_place == 0 && track_cross_place == 0) { // If no flex align is set by styles
         main_place = LV_FLEX_ALIGN_START; 
         cross_place = LV_FLEX_ALIGN_START; 
         track_cross_place = LV_FLEX_ALIGN_START;
    }

    item = cJSON_GetObjectItemCaseSensitive(layout_config, "distribution");
    if (cJSON_IsString(item) && (item->valuestring != NULL)) {
        const char* dist_str = item->valuestring;
        if (strcmp(dist_str, "space_evenly") == 0) main_place = LV_FLEX_ALIGN_SPACE_EVENLY;
        else if (strcmp(dist_str, "space_between") == 0) main_place = LV_FLEX_ALIGN_SPACE_BETWEEN;
        else if (strcmp(dist_str, "space_around") == 0) main_place = LV_FLEX_ALIGN_SPACE_AROUND;
        else if (strcmp(dist_str, "center") == 0) main_place = LV_FLEX_ALIGN_CENTER;
        else if (strcmp(dist_str, "end") == 0) main_place = LV_FLEX_ALIGN_END;
        else if (strcmp(dist_str, "start") == 0) main_place = LV_FLEX_ALIGN_START;
    }

    item = cJSON_GetObjectItemCaseSensitive(layout_config, "item_alignment");
    if (cJSON_IsString(item)) {
        const char* align_str = item->valuestring;
        if (flow == LV_FLEX_FLOW_COLUMN) { // Main: Vertical, Cross: Horizontal
            if (strstr(align_str, "center_horizontal")) cross_place = LV_FLEX_ALIGN_CENTER;
            else if (strstr(align_str, "fill_horizontal")) {
                /* LV_FLEX_ALIGN_STRETCH is preferred here but seems unavailable in your setup.
                 * Defaulting to LV_FLEX_ALIGN_START.
                 * To achieve fill/stretch for children on the cross-axis (horizontal here),
                 * ensure child elements in JSON have their 'width' set to "100%".
                 */
                cross_place = LV_FLEX_ALIGN_START; // WORKAROUND for missing LV_FLEX_ALIGN_STRETCH
                ESP_LOGW(TAG, "Workaround: LV_FLEX_ALIGN_STRETCH unavailable, using START for fill_horizontal. Set child width to \"100%%\" in JSON to stretch.");
            }
            else if (strstr(align_str, "start_horizontal")) cross_place = LV_FLEX_ALIGN_START;
            else if (strstr(align_str, "end_horizontal")) cross_place = LV_FLEX_ALIGN_END;
        } else { // flow == LV_FLEX_FLOW_ROW. Main: Horizontal, Cross: Vertical
            if (strstr(align_str, "center_vertical")) cross_place = LV_FLEX_ALIGN_CENTER;
            else if (strstr(align_str, "fill_vertical")) {
                /* LV_FLEX_ALIGN_STRETCH is preferred here. See note above.
                 * To achieve fill/stretch for children on the cross-axis (vertical here),
                 * ensure child elements in JSON have their 'height' set to "100%".
                 */
                cross_place = LV_FLEX_ALIGN_START; // WORKAROUND
                ESP_LOGW(TAG, "Workaround: LV_FLEX_ALIGN_STRETCH unavailable, using START for fill_vertical. Set child height to \"100%%\" in JSON to stretch.");
            }
            else if (strstr(align_str, "start_vertical")) cross_place = LV_FLEX_ALIGN_START;
            else if (strstr(align_str, "end_vertical")) cross_place = LV_FLEX_ALIGN_END;
        }
    }
    lv_obj_set_flex_align(container, main_place, cross_place, track_cross_place);
}


static lv_obj_t* create_widget_from_json(lv_obj_t *parent, cJSON *widget_config) {
    if (!parent || !widget_config) {
        ESP_LOGE(TAG, "Null parent or widget_config");
        return NULL;
    }

    cJSON *type_json = cJSON_GetObjectItemCaseSensitive(widget_config, "type");
    if (!cJSON_IsString(type_json) || (type_json->valuestring == NULL)) {
        ESP_LOGE(TAG, "Widget type is missing or not a string");
        return NULL;
    }
    const char *type_str = type_json->valuestring;

    lv_obj_t *obj = NULL;

    if (strcmp(type_str, "label") == 0) {
        obj = lv_label_create(parent);
        if (!obj) return NULL;
        apply_common_styles(obj, widget_config); // Apply styles including font first

        cJSON *text_item = cJSON_GetObjectItemCaseSensitive(widget_config, "text_preview");
        if (!text_item) text_item = cJSON_GetObjectItemCaseSensitive(widget_config, "text");
        
        if (cJSON_IsString(text_item) && text_item->valuestring != NULL) {
            lv_label_set_text(obj, text_item->valuestring);
        } else {
            lv_label_set_text(obj, "[Label]");
        }
        cJSON *h_align = cJSON_GetObjectItemCaseSensitive(widget_config, "horizontal_alignment");
        if(cJSON_IsString(h_align) && h_align->valuestring != NULL) {
            if(strcmp(h_align->valuestring, "center") == 0) lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
            else if(strcmp(h_align->valuestring, "left") == 0) lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, 0);
            else if(strcmp(h_align->valuestring, "right") == 0) lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, 0);
        }

    } else if (strcmp(type_str, "button") == 0) {
        obj = lv_btn_create(parent);
        if (!obj) return NULL;
        apply_common_styles(obj, widget_config); // Apply styles to button itself (includes text_color, font from style_ref)

        lv_obj_t *label_on_btn = lv_label_create(obj);
        // The label will inherit text properties from the button's style.
        // No need for: lv_obj_add_style(label_on_btn, (lv_style_t*)lv_obj_get_style_list(obj, LV_PART_MAIN)->style, 0); // THIS LINE WAS REMOVED

        cJSON *btn_label_item = cJSON_GetObjectItemCaseSensitive(widget_config, "label");
        if (cJSON_IsString(btn_label_item) && btn_label_item->valuestring != NULL) {
            lv_label_set_text(label_on_btn, btn_label_item->valuestring);
        } else {
            lv_label_set_text(label_on_btn, "[Button]");
        }
        lv_obj_center(label_on_btn);

        cJSON *action_id_item = cJSON_GetObjectItemCaseSensitive(widget_config, "action_id");
        if (cJSON_IsString(action_id_item) && action_id_item->valuestring != NULL) {
            ESP_LOGI(TAG, "Button '%s' has action_id: %s", lv_label_get_text(label_on_btn), action_id_item->valuestring);
            // TODO: Implement action handling based on action_id
        }

    } else if (strcmp(type_str, "container") == 0) {
        obj = lv_obj_create(parent);
        if (!obj) return NULL;
        apply_common_styles(obj, widget_config); // Apply common styles (bg, padding, etc.) to the container itself

        cJSON *layout_json = cJSON_GetObjectItemCaseSensitive(widget_config, "layout");
        if (layout_json) {
            apply_layout_to_container(obj, layout_json); // Setup layout for children
        } else { 
            // Default layout for a container if not specified in JSON
            lv_obj_set_layout(obj, LV_LAYOUT_FLEX);
            lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN); // No padding by default for generic container
        }

        cJSON *elements_array = cJSON_GetObjectItemCaseSensitive(widget_config, "elements");
        if (cJSON_IsArray(elements_array)) {
            cJSON *child_config = NULL;
            cJSON_ArrayForEach(child_config, elements_array) {
                create_widget_from_json(obj, child_config); // Recursive call for children
            }
        }
        // For containers, common styles were applied before layout and children. This should be fine.
        return obj; // Return directly for containers as all styling and layout is handled.
    } else {
        ESP_LOGW(TAG, "Unsupported widget type: %s", type_str);
        obj = lv_label_create(parent); // Placeholder for unknown types
        if (!obj) return NULL;
        apply_common_styles(obj, widget_config); // Apply basic styles even to placeholder

        char unknown_buf[64];
        snprintf(unknown_buf, sizeof(unknown_buf), "[Unknown: %s]", type_str);
        lv_label_set_text(obj, unknown_buf);
        // Ensure placeholder has a visible background and text color if default is transparent/same as parent
        lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_50, 0);
        lv_obj_set_style_text_color(obj, lv_color_white(), 0);
    }

    // This final call to apply_common_styles is redundant if all branches call it,
    // but kept as a safeguard. Label and Button types now call it early.
    // If obj is NULL here (e.g. unsupported type not creating a placeholder), it's handled.
    // if (obj) {
    //     apply_common_styles(obj, widget_config);
    // }
    return obj;
}

bool my_ui_render_from_json(const char *json_string) {
    if (!json_string) {
        ESP_LOGE(TAG, "JSON string is NULL");
        return false;
    }

    ESP_LOGD(TAG, "Attempting to render UI from JSON (first 256 chars): %.256s", json_string);

    cJSON *root_json = cJSON_Parse(json_string);
    if (root_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "JSON Parse Error before: %s", error_ptr);
        } else {
            ESP_LOGE(TAG, "JSON Parse Error (unknown reason or empty string)");
        }
        return false;
    }

    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
         ESP_LOGE(TAG, "No active screen!");
         cJSON_Delete(root_json);
         return false;
    }
    lv_obj_clean(screen); // Clear previous UI from the screen

    // Apply screen-level configurations
    cJSON *screen_bg_color = cJSON_GetObjectItemCaseSensitive(root_json, "background_color");
    if (cJSON_IsString(screen_bg_color) && screen_bg_color->valuestring != NULL) {
        lv_obj_set_style_bg_color(screen, parse_color_string(screen_bg_color->valuestring), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    } else {
         lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN); // Default screen background
         lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    }

    cJSON *root_layout_json = cJSON_GetObjectItemCaseSensitive(root_json, "layout");
    if (root_layout_json) {
        apply_layout_to_container(screen, root_layout_json); // Screen itself can be a flex container
    } else { 
        // Default layout for the screen if no root layout specified in JSON
        lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(screen, 5, LV_PART_MAIN); // Some default padding for the screen
        lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }

    cJSON *elements_array = cJSON_GetObjectItemCaseSensitive(root_json, "elements");
    if (cJSON_IsArray(elements_array)) {
        cJSON *element_config = NULL;
        cJSON_ArrayForEach(element_config, elements_array) {
            create_widget_from_json(screen, element_config);
        }
    } else {
        ESP_LOGW(TAG, "No 'elements' array found in root JSON object or it's not an array.");
    }

    cJSON_Delete(root_json);
    ESP_LOGI(TAG, "UI rendering from JSON finished.");
    return true;
}