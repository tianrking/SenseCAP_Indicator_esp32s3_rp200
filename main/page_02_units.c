// page_02_units.c
#include "page_02_units.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "page_units";

/**
 * @brief Initialize the UI elements for the Unit Converter tool.
 *
 * @param parent The parent LVGL object (typically a tab page).
 */
void page_02_units_init(lv_obj_t *parent)
{
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL!");
        return;
    }

    ESP_LOGI(TAG, "Initializing Units Page UI...");

    // --- Create UI elements for the Unit Converter tool within the 'parent' object ---

    // Example: Add a simple placeholder label
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Unit Converter Tool\n(Content to be added)");
    lv_obj_center(label); // Center the label on the page

    // TODO: Implement the Unit Converter UI here.
    // - Use lv_dropdown for unit selection.
    // - Use lv_textarea/lv_spinbox for input value.
    // - Use lv_label for output value.

    ESP_LOGI(TAG, "Units Page UI Initialized.");
}