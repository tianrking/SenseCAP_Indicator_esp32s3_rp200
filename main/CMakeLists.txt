set(LV_DEMO_DIR ../components/lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

idf_component_register(
    SRCS "main.c" "lv_port.c" "my_ui.c" ${LV_DEMOS_SOURCES} ${LV_DEMO1_SOURCES} ${LV_DEMO2_SOURCES}
    INCLUDE_DIRS "." ${LV_DEMO_DIR} )
