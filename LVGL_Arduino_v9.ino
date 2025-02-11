/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/get-started/platforms/arduino.html
 Install: lvgl*/

#include <lvgl.h>
#include <ctime>

/*To use the built-in examples and demos of LVGL uncomment the includes below respectively.
 *You also need to copy `lvgl/examples` to `lvgl/src/examples`. Similarly for the demos `lvgl/demos` to `lvgl/src/demos`.*/

 /*******************************************************************************
  * LVGL Configuration file:
  * Copy your_arduino_path/libraries/lvgl/lv_conf_template.h
  * to your_arduino_path/libraries/lv_conf.h
  *
  * In lv_conf.h around line 15, enable config file:
  * #if 1 // Set it to "1" to enable content
  *
  * Then find and set:
  * #define LV_COLOR_DEPTH     16
  * #define LV_TICK_CUSTOM     1
  *
  * For SPI/parallel 8 display set color swap can be faster, parallel 16/RGB screen don't swap!
  * #define LV_COLOR_16_SWAP   0 // for parallel 16 and RGB
  *
  * Enable LVGL Demo Widgets
  * #define LV_USE_DEMO_WIDGETS 1
  *
  * Optional: enable LVGL Demo Benchmark:
  * #define LV_USE_DEMO_BENCHMARK 0
  *
  * Optional: enable the usage of encoder and keyboard
  * #define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
  *
  * Optional: enable music player demo
  * #define LV_USE_DEMO_MUSIC 0
  *
  * Optional: enable stress test for LVGL
  * #define LV_USE_DEMO_STRESS 0
  *
  * Enables support for compressed fonts:
  * #define LV_USE_FONT_COMPRESSED 1
  *
  * Customize font size:
  * #define LV_FONT_MONTSERRAT_12 1
  * #define LV_FONT_MONTSERRAT_14 1
  * #define LV_FONT_MONTSERRAT_24 1
    #define LV_FONT_MONTSERRAT_32 1 // Added larger font size for local time
  * #define LV_FONT_DEFAULT &lv_font_montserrat_12
  *
  * Optional: Show CPU usage and FPS count
  * #define LV_USE_PERF_MONITOR 1
 ******************************************************************************/

 //#include "./demos/lv_demos.h" // 注释掉演示代码

#define DIRECT_MODE // Uncomment to enable full frame buffer

#include "Indicator_SWSPI.h"

/* Install: GFX Library for Arduino*/
#include <Arduino_GFX_Library.h>

/* Install: Anitracks_PCA95x5*/
#include <PCA95x5.h>

#define GFX_BL DF_GFX_BL // default backlight pin, you may replace DF_GFX_BL to actual backlight pin

#if defined(DISPLAY_DEV_KIT)
Arduino_GFX *gfx = create_default_Arduino_GFX();
#else /* !defined(DISPLAY_DEV_KIT) */

#define GFX_DEV_DEVICE ESP32_S3_RGB
#define GFX_BL 45
Arduino_DataBus *bus = new Indicator_SWSPI(
    GFX_NOT_DEFINED /* DC */, PCA95x5::Port::P04 /* CS */,
    41 /* SCK */, 48 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
    4 /* R0 */, 3 /* R1 */, 2 /* R2 */, 1 /* R3 */, 0 /* R4 */,
    10 /* G0 */, 9 /* G1 */, 8 /* G2 */, 7 /* G3 */, 6 /* G4 */, 5 /* G5 */,
    15 /* B0 */, 14 /* B1 */, 13 /* B2 */, 12 /* B3 */, 11 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 2 /* rotation */, false /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

#endif /* !defined(DISPLAY_DEV_KIT) */
 /*******************************************************************************
  * End of Arduino_GFX setting
  ******************************************************************************/

  /*******************************************************************************
   * Please config the touch panel in touch.h

   * Install the correct touchlib
   * Location:   github:https://github.com/mmMicky/TouchLib
   * ZIP:        https://github.com/mmMicky/TouchLib/archive/refs/heads/main.zip
   ******************************************************************************/
#include "touch.h"

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf)
{
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

uint32_t millis_cb(void)
{
    return millis();
}

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
#ifndef DIRECT_MODE
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif // #ifndef DIRECT_MODE

    /*Call it to tell LVGL you are ready*/
    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (touch_has_signal())
    {
        if (touch_touched())
        {
            data->state = LV_INDEV_STATE_PRESSED;

            /*Set the coordinates*/
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        }
        else if (touch_released())
        {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// 定义时区和城市名称（英文）
typedef struct {
    const char *city;
    const char *timezone;
    lv_obj_t *label;
    lv_obj_t *time_label; // 单独的时间标签
} CityTime;

// 本地时间（香港）
lv_obj_t *local_time_label;

CityTime cities[] = {
    {"Barcelona", "GMT+1", NULL, NULL},  // 巴塞罗那
    {"New York", "GMT-5", NULL, NULL},   // 纽约
    {"Tokyo", "GMT+9", NULL, NULL},      // 东京
    {"Istanbul", "GMT+3", NULL, NULL},   // 伊斯坦布尔
};

// 更新香港时间的函数
static void update_hongkong_time(lv_timer_t *timer) {
    time_t now;
    struct tm timeinfo;
    char buf[64];

    setenv("TZ", "GMT+8", 1); // 香港时间
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    lv_label_set_text(local_time_label, buf);
    lv_obj_center(local_time_label); // 居中显示
}

// 更新其他城市时间的函数
static void update_all_city_time(lv_timer_t *timer) {
    time_t now;
    struct tm timeinfo;
    char buf[64];

    for (int i = 0; i < sizeof(cities) / sizeof(cities[0]); i++)
    {
        setenv("TZ", cities[i].timezone, 1);
        tzset();
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
        lv_label_set_text(cities[i].time_label, buf); // 更新时间标签
    }
}


void setup() {
  #ifdef DEV_DEVICE_INIT
    DEV_DEVICE_INIT();
  #endif

    Serial.begin(115200);
    // Serial.setDebugOutput(true);
    // while(!Serial);
    Serial.println("SenseCap Indicator GFX LVGL_Arduino_v9 example ");
    String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println(LVGL_Arduino);

    // Init Display
    if (!gfx->begin())
    {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

  #ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
  #endif

    // Init touch device
    touch_init(gfx->width(), gfx->height(), gfx->getRotation());

    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(millis_cb);

    /* register print function for debugging */
  #if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
  #endif

    screenWidth = gfx->width();
    screenHeight = gfx->height();

  #ifdef DIRECT_MODE
    bufSize = screenWidth * screenHeight;
  #else
    bufSize = screenWidth * 40;
  #endif

  #ifdef ESP32
    #if defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL))
    disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
    #else  // !(defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL)))
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf)
    {
        // remove MALLOC_CAP_INTERNAL flag try again
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
    }
    #endif // !(defined(DIRECT_MODE) && (defined(CANVAS) || defined(RGB_PANEL)))
  #else // !ESP32
    Serial.println("LVGL disp_draw_buf heap_caps_malloc failed! malloc again...");
    disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
  #endif // !ESP32
    if (!disp_draw_buf)
    {
        Serial.println("LVGL disp_draw_buf allocate failed!");
    }
    else
    {
        disp = lv_display_create(screenWidth, screenHeight);
        lv_display_set_flush_cb(disp, my_disp_flush);
    #ifdef DIRECT_MODE
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
    #else
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
    #endif

        /*Initialize the (dummy) input device driver*/
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
        lv_indev_set_read_cb(indev, my_touchpad_read);

        // lv_demo_widgets(); // 注释掉演示代码

        // 设置背景颜色
        lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x222222), LV_PART_MAIN);

        // 创建本地时间（香港时间）标签
        local_time_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(local_time_label, &lv_font_montserrat_32, 0); // 使用更大的字体
        lv_obj_set_style_text_color(local_time_label, lv_color_hex(0xFFFFFF), 0); // 白色

        // 创建其他城市时间标签和城市名标签
         int label_height = 30;
        int padding = 10;
        int start_y = screenHeight / 2 + 20; // 从屏幕中间偏下一点开始

        lv_color_t colors[] = {
          lv_color_hex(0xFF9933), // Orange
          lv_color_hex(0x3399FF), // Blue
          lv_color_hex(0x99FF33), // Green
          lv_color_hex(0xFF3399)  // Pink
        };

        for (int i = 0; i < sizeof(cities) / sizeof(cities[0]); i++) {
            // 城市名标签
            cities[i].label = lv_label_create(lv_scr_act());
            lv_obj_set_style_text_font(cities[i].label, &lv_font_montserrat_14, 0); // 使用稍小的字体
            lv_obj_set_style_text_color(cities[i].label, colors[i], 0);
            lv_label_set_text(cities[i].label, cities[i].city);
            lv_obj_set_width(cities[i].label, screenWidth / 2 - 20);
            lv_obj_set_pos(cities[i].label, (i % 2) * (screenWidth / 2) + 10, start_y + (i / 2) * (label_height + padding));

            // 时间标签
            cities[i].time_label = lv_label_create(lv_scr_act());
            lv_obj_set_style_text_font(cities[i].time_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(cities[i].time_label, colors[i], 0);
          lv_obj_set_width(cities[i].time_label, screenWidth/2 -20);
          lv_obj_set_pos(cities[i].time_label, (i%2) * (screenWidth / 2) + 10, start_y + (i/2) * (label_height + padding) + 20); // 时间标签放在城市名下方

        }

        // 创建定时器，每秒更新一次时间
        lv_timer_create(update_hongkong_time, 1000, NULL);
        lv_timer_create(update_all_city_time, 1000, NULL);


        // 首次调用，立即显示时间
        update_hongkong_time(NULL);
        update_all_city_time(NULL);
    }

    Serial.println("Setup done");
}

void loop() {
  lv_task_handler(); /* let the GUI do its work */

  #ifdef DIRECT_MODE
    #if defined(CANVAS) || defined(RGB_PANEL)
    gfx->flush();
    #else // !(defined(CANVAS) || defined(RGB_PANEL))
    gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
    #endif // !(defined(CANVAS) || defined(RGB_PANEL))
  #else  // !DIRECT_MODE
    #ifdef CANVAS
    gfx->flush();
    #endif
  #endif // !DIRECT_MODE

    delay(5);
}