// my_ui.c
#include "my_ui.h"
#include "lvgl.h"
#include "esp_log.h" // For logging

// Include header files for each page (tool)
#include "page_01_formulas.h"
#include "page_02_units.h"
#include "page_03_kinematics.h"
#include "page_04_projectile.h"
#include "page_05_shm.h"
#include "page_06_optics.h"
#include "page_07_circuits.h"
// #include "page_08_..." // Add more pages later if needed

static const char *TAG = "my_ui";

/**
 * @brief Initialize the main user interface.
 */
void my_ui_init(void)
{
    ESP_LOGI(TAG, "Initializing Main UI with Tabview...");

    // Get the active screen
    lv_obj_t *scr = lv_scr_act();

    // 1. Create the Tabview object
    lv_obj_t *tabview = lv_tabview_create(scr, LV_DIR_TOP, 50); // Keep tab height
    if (!tabview) {
        ESP_LOGE(TAG, "Failed to create tabview!");
        return;
    }

    // --- BEGIN MODIFICATION: Disable content swipe for tab change ---
    // Get the content area container object of the tabview
    lv_obj_t *tab_content = lv_tabview_get_content(tabview);
    if (tab_content) {
        // Clear flags that enable swipe-based tab switching and scrolling on the content area itself
        // LV_OBJ_FLAG_SNAPPABLE is often used for swipe navigation in containers.
        // Clearing SCROLLABLE prevents accidental vertical scrolls on the content area if not needed.
        lv_obj_clear_flag(tab_content, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SNAPPABLE);
        ESP_LOGI(TAG, "Disabled swipe navigation on tabview content area.");
    } else {
         ESP_LOGW(TAG, "Could not get tabview content object to disable swipe.");
    }
    // --- END MODIFICATION ---


    // 2. Add tabs with SHORTER names for each tool/page
    lv_obj_t *tab1_page = lv_tabview_add_tab(tabview, "Formula");
    lv_obj_t *tab2_page = lv_tabview_add_tab(tabview, "Units");
    lv_obj_t *tab3_page = lv_tabview_add_tab(tabview, "Kinema");
    lv_obj_t *tab4_page = lv_tabview_add_tab(tabview, "Proj");
    lv_obj_t *tab5_page = lv_tabview_add_tab(tabview, "SHM");
    lv_obj_t *tab6_page = lv_tabview_add_tab(tabview, "Optics");
    // lv_obj_t *tab7_page = lv_tabview_add_tab(tabview, "Circuit");
    lv_obj_t *tab7_page = lv_tabview_add_tab(tabview, "Button");

    // 3. Call the initialization function for each page, passing its container
    if (tab1_page) { page_01_formulas_init(tab1_page); ESP_LOGD(TAG, "Page 1 (Formula) initialized."); }
    else { ESP_LOGE(TAG, "Failed to get page for tab 1"); }

    if (tab2_page) { page_02_units_init(tab2_page); ESP_LOGD(TAG, "Page 2 (Units) initialized."); }
     else { ESP_LOGE(TAG, "Failed to get page for tab 2"); }

    if (tab3_page) { page_03_kinematics_init(tab3_page); ESP_LOGD(TAG, "Page 3 (Kinema) initialized."); }
     else { ESP_LOGE(TAG, "Failed to get page for tab 3"); }

    if (tab4_page) { page_04_projectile_init(tab4_page); ESP_LOGD(TAG, "Page 4 (Proj) initialized."); }
     else { ESP_LOGE(TAG, "Failed to get page for tab 4"); }

     if (tab5_page) { page_05_shm_init(tab5_page); ESP_LOGD(TAG, "Page 5 (SHM) initialized."); }
     else { ESP_LOGE(TAG, "Failed to get page for tab 5"); }

     if (tab6_page) { page_06_optics_init(tab6_page); ESP_LOGD(TAG, "Page 6 (Optics) initialized."); }
      else { ESP_LOGE(TAG, "Failed to get page for tab 6"); }

     // Note: page_07_circuits_init itself should NOT clear LV_OBJ_FLAG_SCROLLABLE on the 'parent'
     // it receives (tab7_page) because we already cleared it globally on the tab_content.
     // If page 7 internally needs a scrollable area, it should create its own child container
     // and make THAT scrollable.
     if (tab7_page) {
         // We already disabled scroll on the main content area.
         // If page 7 *specifically* needs scrolling for its *own content*,
         // it must manage that internally by creating a scrollable child object.
         // The page itself (tab7_page) should remain non-scrollable.
         // So, the lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE) inside
         // page_07_circuits_init should ideally be removed if it exists.
         page_07_circuits_init(tab7_page);
         ESP_LOGD(TAG, "Page 7 (Circuit) initialized.");
     } else { ESP_LOGE(TAG, "Failed to get page for tab 7"); }

    ESP_LOGI(TAG, "Main UI Initialization Complete.");
}