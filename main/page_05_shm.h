// page_05_shm.h
#ifndef PAGE_05_SHM_H
#define PAGE_05_SHM_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the UI elements for the Simple Harmonic Motion (SHM) Simulator.
 *
 * @param parent The parent LVGL object (typically a tab page) to which UI elements will be added.
 */
void page_05_shm_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PAGE_05_SHM_H*/