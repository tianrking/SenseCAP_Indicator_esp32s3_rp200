// page_06_optics.h
#ifndef PAGE_06_OPTICS_H
#define PAGE_06_OPTICS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI elements for the Optics Simulator (Snell's Law).
 *
 * @param parent The parent LVGL object (typically a tab page) to which UI elements will be added.
 */
void page_06_optics_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PAGE_06_OPTICS_H*/