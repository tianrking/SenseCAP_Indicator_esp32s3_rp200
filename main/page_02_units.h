// page_02_units.h
#ifndef PAGE_02_UNITS_H
#define PAGE_02_UNITS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI elements for the Unit Converter tool.
 *
 * @param parent The parent LVGL object (typically a tab page) to which UI elements will be added.
 */
void page_02_units_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PAGE_02_UNITS_H*/