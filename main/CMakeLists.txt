set(LV_DEMO_DIR ../components/lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

# set(LV_DEMO1_DIR ./demo1)
# file(GLOB_RECURSE LV_DEMO1_SOURCES ${LV_DEMO1_DIR}/*.c)

# set(LV_DEMO2_DIR ./demo2)
# file(GLOB_RECURSE LV_DEMO2_SOURCES ${LV_DEMO2_DIR}/*.c)

idf_component_register(
    SRCS "main.c" "lv_port.c" "my_ui.c" "page_01_formulas.c" "page_02_units.c" "page_03_kinematics.c" "page_04_projectile.c" "page_05_shm.c"  "page_06_optics.c" "page_07_circuits.c"  ${LV_DEMOS_SOURCES} ${LV_DEMO1_SOURCES} ${LV_DEMO2_SOURCES}
    INCLUDE_DIRS "." ${LV_DEMO_DIR} )
