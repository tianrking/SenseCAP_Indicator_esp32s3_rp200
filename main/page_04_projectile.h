// page_04_projectile.h
#ifndef PAGE_04_PROJECTILE_H
#define PAGE_04_PROJECTILE_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI elements for the Projectile Motion Simulator.
 *
 * @param parent The parent LVGL object (typically a tab page) to which UI elements will be added.
 */
void page_04_projectile_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PAGE_04_PROJECTILE_H*/