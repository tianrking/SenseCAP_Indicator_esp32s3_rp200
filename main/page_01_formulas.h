// page_01_formulas.h
#ifndef PAGE_01_FORMULAS_H
#define PAGE_01_FORMULAS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI elements for the Physics Formulas tool.
 *
 * @param parent The parent LVGL object (typically a tab page) to which UI elements will be added.
 */
void page_01_formulas_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PAGE_01_FORMULAS_H*/