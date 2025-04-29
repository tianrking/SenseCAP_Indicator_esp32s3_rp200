// page_03_kinematics.h
#ifndef PAGE_03_KINEMATICS_H
#define PAGE_03_KINEMATICS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI elements for the 1D Kinematics Simulator.
 *
 * @param parent The parent LVGL object (typically a tab page) to which UI elements will be added.
 */
void page_03_kinematics_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PAGE_03_KINEMATICS_H*/