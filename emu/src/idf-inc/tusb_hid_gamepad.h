#ifndef _TUSB_HID_GAMEPAD_H_
#define _TUSB_HID_GAMEPAD_H_

#include <stdint.h>
#include <stdbool.h>

#define TU_BIT(n)             (1UL << (n))
#define TU_ATTR_PACKED                __attribute__ ((packed))

/// HID Request Report Type
typedef enum
{
  HID_REPORT_TYPE_INVALID = 0,
  HID_REPORT_TYPE_INPUT,      ///< Input
  HID_REPORT_TYPE_OUTPUT,     ///< Output
  HID_REPORT_TYPE_FEATURE     ///< Feature
}hid_report_type_t;

/// Standard Gamepad Buttons Bitmap
typedef enum
{
  GAMEPAD_BUTTON_0  = TU_BIT(0),
  GAMEPAD_BUTTON_1  = TU_BIT(1),
  GAMEPAD_BUTTON_2  = TU_BIT(2),
  GAMEPAD_BUTTON_3  = TU_BIT(3),
  GAMEPAD_BUTTON_4  = TU_BIT(4),
  GAMEPAD_BUTTON_5  = TU_BIT(5),
  GAMEPAD_BUTTON_6  = TU_BIT(6),
  GAMEPAD_BUTTON_7  = TU_BIT(7),
  GAMEPAD_BUTTON_8  = TU_BIT(8),
  GAMEPAD_BUTTON_9  = TU_BIT(9),
  GAMEPAD_BUTTON_10 = TU_BIT(10),
  GAMEPAD_BUTTON_11 = TU_BIT(11),
  GAMEPAD_BUTTON_12 = TU_BIT(12),
  GAMEPAD_BUTTON_13 = TU_BIT(13),
  GAMEPAD_BUTTON_14 = TU_BIT(14),
  GAMEPAD_BUTTON_15 = TU_BIT(15),
  GAMEPAD_BUTTON_16 = TU_BIT(16),
  GAMEPAD_BUTTON_17 = TU_BIT(17),
  GAMEPAD_BUTTON_18 = TU_BIT(18),
  GAMEPAD_BUTTON_19 = TU_BIT(19),
  GAMEPAD_BUTTON_20 = TU_BIT(20),
  GAMEPAD_BUTTON_21 = TU_BIT(21),
  GAMEPAD_BUTTON_22 = TU_BIT(22),
  GAMEPAD_BUTTON_23 = TU_BIT(23),
  GAMEPAD_BUTTON_24 = TU_BIT(24),
  GAMEPAD_BUTTON_25 = TU_BIT(25),
  GAMEPAD_BUTTON_26 = TU_BIT(26),
  GAMEPAD_BUTTON_27 = TU_BIT(27),
  GAMEPAD_BUTTON_28 = TU_BIT(28),
  GAMEPAD_BUTTON_29 = TU_BIT(29),
  GAMEPAD_BUTTON_30 = TU_BIT(30),
  GAMEPAD_BUTTON_31 = TU_BIT(31),
}hid_gamepad_button_bm_t;

/// Standard Gamepad Buttons Naming from Linux input event codes
/// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
#define GAMEPAD_BUTTON_A       GAMEPAD_BUTTON_0
#define GAMEPAD_BUTTON_SOUTH   GAMEPAD_BUTTON_0

#define GAMEPAD_BUTTON_B       GAMEPAD_BUTTON_1
#define GAMEPAD_BUTTON_EAST    GAMEPAD_BUTTON_1

#define GAMEPAD_BUTTON_C       GAMEPAD_BUTTON_2

#define GAMEPAD_BUTTON_X       GAMEPAD_BUTTON_3
#define GAMEPAD_BUTTON_NORTH   GAMEPAD_BUTTON_3

#define GAMEPAD_BUTTON_Y       GAMEPAD_BUTTON_4
#define GAMEPAD_BUTTON_WEST    GAMEPAD_BUTTON_4

#define GAMEPAD_BUTTON_Z       GAMEPAD_BUTTON_5
#define GAMEPAD_BUTTON_TL      GAMEPAD_BUTTON_6
#define GAMEPAD_BUTTON_TR      GAMEPAD_BUTTON_7
#define GAMEPAD_BUTTON_TL2     GAMEPAD_BUTTON_8
#define GAMEPAD_BUTTON_TR2     GAMEPAD_BUTTON_9
#define GAMEPAD_BUTTON_SELECT  GAMEPAD_BUTTON_10
#define GAMEPAD_BUTTON_START   GAMEPAD_BUTTON_11
#define GAMEPAD_BUTTON_MODE    GAMEPAD_BUTTON_12
#define GAMEPAD_BUTTON_THUMBL  GAMEPAD_BUTTON_13
#define GAMEPAD_BUTTON_THUMBR  GAMEPAD_BUTTON_14

#define GAMEPAD_NS_BUTTON_Y       0x01
#define GAMEPAD_NS_BUTTON_B       0x02
#define GAMEPAD_NS_BUTTON_A       0x04
#define GAMEPAD_NS_BUTTON_X       0x08
#define GAMEPAD_NS_BUTTON_TL      0x10
#define GAMEPAD_NS_BUTTON_TR      0x20
#define GAMEPAD_NS_BUTTON_TL2     0x40
#define GAMEPAD_NS_BUTTON_TR2     0x80
#define GAMEPAD_NS_BUTTON_SELECT  0x100
#define GAMEPAD_NS_BUTTON_START   0x200
#define GAMEPAD_NS_BUTTON_THUMBL  0x400
#define GAMEPAD_NS_BUTTON_THUMBR  0x800
#define GAMEPAD_NS_BUTTON_MODE    0x1000
#define GAMEPAD_NS_BUTTON_C       0x2000
#define GAMEPAD_NS_BUTTON_Z       0x4000

/// Standard Gamepad HAT/DPAD Buttons (from Linux input event codes)
typedef enum
{
  GAMEPAD_HAT_CENTERED   = 0,  ///< DPAD_CENTERED
  GAMEPAD_HAT_UP         = 1,  ///< DPAD_UP
  GAMEPAD_HAT_UP_RIGHT   = 2,  ///< DPAD_UP_RIGHT
  GAMEPAD_HAT_RIGHT      = 3,  ///< DPAD_RIGHT
  GAMEPAD_HAT_DOWN_RIGHT = 4,  ///< DPAD_DOWN_RIGHT
  GAMEPAD_HAT_DOWN       = 5,  ///< DPAD_DOWN
  GAMEPAD_HAT_DOWN_LEFT  = 6,  ///< DPAD_DOWN_LEFT
  GAMEPAD_HAT_LEFT       = 7,  ///< DPAD_LEFT
  GAMEPAD_HAT_UP_LEFT    = 8,  ///< DPAD_UP_LEFT
}hid_gamepad_hat_t;

/// Switch Gamepad HAT/DPAD Buttons (from Linux input event codes)
typedef enum
{
  GAMEPAD_NS_HAT_CENTERED   = 8,  ///< DPAD_CENTERED
  GAMEPAD_NS_HAT_UP         = 0,  ///< DPAD_UP
  GAMEPAD_NS_HAT_UP_RIGHT   = 1,  ///< DPAD_UP_RIGHT
  GAMEPAD_NS_HAT_RIGHT      = 2,  ///< DPAD_RIGHT
  GAMEPAD_NS_HAT_DOWN_RIGHT = 3,  ///< DPAD_DOWN_RIGHT
  GAMEPAD_NS_HAT_DOWN       = 4,  ///< DPAD_DOWN
  GAMEPAD_NS_HAT_DOWN_LEFT  = 5,  ///< DPAD_DOWN_LEFT
  GAMEPAD_NS_HAT_LEFT       = 6,  ///< DPAD_LEFT
  GAMEPAD_NS_HAT_UP_LEFT    = 7,  ///< DPAD_UP_LEFT
}hid_gamepad_ns_hat_t;

/// HID Gamepad Protocol Report.
typedef struct TU_ATTR_PACKED
{
  int8_t  x;         ///< Delta x  movement of left analog-stick
  int8_t  y;         ///< Delta y  movement of left analog-stick
  int8_t  z;         ///< Delta z  movement of right analog-joystick
  int8_t  rz;        ///< Delta Rz movement of right analog-joystick
  int8_t  rx;        ///< Delta Rx movement of analog left trigger
  int8_t  ry;        ///< Delta Ry movement of analog right trigger
  uint8_t hat;       ///< Buttons mask for currently pressed buttons in the DPad/hat
  uint32_t buttons;  ///< Buttons mask for currently pressed buttons
}hid_gamepad_report_t;

/// HID Switch Gamepad Protocol Report.
typedef struct TU_ATTR_PACKED
{
  uint16_t buttons;  ///< Buttons mask for currently pressed buttons
  uint8_t hat;       ///< Buttons mask for currently pressed buttons in the DPad/hat
  int8_t  x;         ///< Delta x  movement of left analog-stick
  int8_t  y;         ///< Delta y  movement of left analog-stick
  int8_t  rx;        ///< Delta Rx movement of analog left trigger
  int8_t  ry;        ///< Delta Ry movement of analog right trigger
  int8_t  z;         ///< Delta z  movement of right analog-joystick
  int8_t  rz;        ///< Delta Rz movement of right analog-joystick
}hid_gamepad_ns_report_t;

void tud_gamepad_report(hid_gamepad_report_t * report);
void tud_gamepad_ns_report(hid_gamepad_ns_report_t * report);
bool tud_ready(void);

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t* buffer,
                               uint16_t reqlen);
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const* buffer,
                           uint16_t bufsize);

#endif