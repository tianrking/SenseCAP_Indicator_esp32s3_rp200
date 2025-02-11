#include <PCA95x5.h>

#define TOUCH_MODULES_FT5x06
#define TOUCH_MODULE_ADDR (0x48)

#define TOUCH_SCL 40
#define TOUCH_SDA 39

#define TOUCH_RES PCA95x5::Port::P07
#define TOUCH_INT PCA95x5::Port::P06

// Please fill below values from Arduino_GFX Example - TouchCalibration
bool touch_swap_xy = false;
int16_t touch_map_x1 = -1;
int16_t touch_map_x2 = -1;
int16_t touch_map_y1 = -1;
int16_t touch_map_y2 = -1;

int16_t touch_max_x = 0, touch_max_y = 0;
int16_t touch_raw_x = 0, touch_raw_y = 0;
int16_t touch_last_x = 0, touch_last_y = 0;

#if defined(TOUCH_MODULE_ADDR) // TouchLib
#include <Wire.h>
#include <TouchLib.h>
TouchLib touch(Wire, TOUCH_SDA, TOUCH_SCL, TOUCH_MODULE_ADDR);
#if defined(TOUCH_INT)
bool get_int = false;
#endif // TOUCH_INT
#endif // TouchLib

void touch_init(int16_t w, int16_t h, uint8_t r)
{
  touch_max_x = w - 1;
  touch_max_y = h - 1;
  if (touch_map_x1 == -1)
  {
    switch (r)
    {
    case 3:
      touch_swap_xy = true;
      touch_map_x1 = touch_max_x;
      touch_map_x2 = 0;
      touch_map_y1 = 0;
      touch_map_y2 = touch_max_y;
      break;
    case 2:
      touch_swap_xy = false;
      touch_map_x1 = touch_max_x;
      touch_map_x2 = 0;
      touch_map_y1 = touch_max_y;
      touch_map_y2 = 0;
      break;
    case 1:
      touch_swap_xy = true;
      touch_map_x1 = 0;
      touch_map_x2 = touch_max_x;
      touch_map_y1 = touch_max_y;
      touch_map_y2 = 0;
      break;
    default: // case 0:
      touch_swap_xy = false;
      touch_map_x1 = 0;
      touch_map_x2 = touch_max_x;
      touch_map_y1 = 0;
      touch_map_y2 = touch_max_y;
      break;
    }
  }

#if defined(TOUCH_MODULE_ADDR) // TouchLib
  // Reset touchscreen
#if defined(TOUCH_RES)
  pinMode(TOUCH_RES, OUTPUT);
  digitalWrite(TOUCH_RES, 0);
  delay(200);
  digitalWrite(TOUCH_RES, 1);
  delay(200);
#endif
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  touch.init();
#if defined(TOUCH_INT)
  attachInterrupt(
      TOUCH_INT,
      []
      {
        get_int = 1;
      },
      CHANGE);
#endif // TOUCH_INT
#endif // TouchLib
}

bool touch_has_signal()
{
#if defined(TOUCH_MODULE_ADDR) // TouchLib
#if defined(TOUCH_INT)
  if (get_int)
  {
    get_int = 0;
    return true;
  }
#else
  return true;
#endif                           // TOUCH_INT
#endif                           // TouchLib
  return false;
}

void translate_touch_raw()
{
  if (touch_swap_xy)
  {
    touch_last_x = map(touch_raw_y, touch_map_x1, touch_map_x2, 0, touch_max_x);
    touch_last_y = map(touch_raw_x, touch_map_y1, touch_map_y2, 0, touch_max_y);
  }
  else
  {
    touch_last_x = map(touch_raw_x, touch_map_x1, touch_map_x2, 0, touch_max_x);
    touch_last_y = map(touch_raw_y, touch_map_y1, touch_map_y2, 0, touch_max_y);
  }
}

bool touch_touched()
{
#if defined(TOUCH_MODULE_ADDR) // TouchLib
  if (touch.read())
  {
    TP_Point t = touch.getPoint(0);
    touch_raw_x = t.x;
    touch_raw_y = t.y;

    touch_last_x = touch_raw_x;
    touch_last_y = touch_raw_y;

    translate_touch_raw();
    return true;
  }
#endif // TouchLib

  return false;
}

bool touch_released()
{
  return false;
}
