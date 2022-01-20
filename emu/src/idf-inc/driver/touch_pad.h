#ifndef _TOUCH_PAD_H_
#define _TOUCH_PAD_H_

#define BIT(x) (1 << x)

typedef enum {
    TOUCH_PAD_INTR_MASK_DONE = BIT(0),      /*!<Measurement done for one of the enabled channels. */
    TOUCH_PAD_INTR_MASK_ACTIVE = BIT(1),    /*!<Active for one of the enabled channels. */
    TOUCH_PAD_INTR_MASK_INACTIVE = BIT(2),  /*!<Inactive for one of the enabled channels. */
    TOUCH_PAD_INTR_MASK_SCAN_DONE = BIT(3), /*!<Measurement done for all the enabled channels. */
    TOUCH_PAD_INTR_MASK_TIMEOUT = BIT(4),   /*!<Timeout for one of the enabled channels. */
#if SOC_TOUCH_PROXIMITY_MEAS_DONE_SUPPORTED
    TOUCH_PAD_INTR_MASK_PROXI_MEAS_DONE = BIT(5),   /*!<For proximity sensor, when the number of measurements reaches the set count of measurements, an interrupt will be generated. */
    TOUCH_PAD_INTR_MASK_MAX
#define TOUCH_PAD_INTR_MASK_ALL (TOUCH_PAD_INTR_MASK_TIMEOUT    \
                                | TOUCH_PAD_INTR_MASK_SCAN_DONE \
                                | TOUCH_PAD_INTR_MASK_INACTIVE  \
                                | TOUCH_PAD_INTR_MASK_ACTIVE    \
                                | TOUCH_PAD_INTR_MASK_DONE      \
                                | TOUCH_PAD_INTR_MASK_PROXI_MEAS_DONE) /*!<All touch interrupt type enable. */
#else
    TOUCH_PAD_INTR_MASK_MAX
#define TOUCH_PAD_INTR_MASK_ALL (TOUCH_PAD_INTR_MASK_TIMEOUT    \
                                | TOUCH_PAD_INTR_MASK_SCAN_DONE \
                                | TOUCH_PAD_INTR_MASK_INACTIVE  \
                                | TOUCH_PAD_INTR_MASK_ACTIVE    \
                                | TOUCH_PAD_INTR_MASK_DONE) /*!<All touch interrupt type enable. */
#endif
} touch_pad_intr_mask_t;

/** Touch pad channel */
typedef enum {
    TOUCH_PAD_NUM0 = 0, /*!< Touch pad channel 0 is GPIO4(ESP32) */
    TOUCH_PAD_NUM1,     /*!< Touch pad channel 1 is GPIO0(ESP32) / GPIO1(ESP32-S2) */
    TOUCH_PAD_NUM2,     /*!< Touch pad channel 2 is GPIO2(ESP32) / GPIO2(ESP32-S2) */
    TOUCH_PAD_NUM3,     /*!< Touch pad channel 3 is GPIO15(ESP32) / GPIO3(ESP32-S2) */
    TOUCH_PAD_NUM4,     /*!< Touch pad channel 4 is GPIO13(ESP32) / GPIO4(ESP32-S2) */
    TOUCH_PAD_NUM5,     /*!< Touch pad channel 5 is GPIO12(ESP32) / GPIO5(ESP32-S2) */
    TOUCH_PAD_NUM6,     /*!< Touch pad channel 6 is GPIO14(ESP32) / GPIO6(ESP32-S2) */
    TOUCH_PAD_NUM7,     /*!< Touch pad channel 7 is GPIO27(ESP32) / GPIO7(ESP32-S2) */
    TOUCH_PAD_NUM8,     /*!< Touch pad channel 8 is GPIO33(ESP32) / GPIO8(ESP32-S2) */
    TOUCH_PAD_NUM9,     /*!< Touch pad channel 9 is GPIO32(ESP32) / GPIO9(ESP32-S2) */
#if SOC_TOUCH_SENSOR_NUM > 10
    TOUCH_PAD_NUM10,    /*!< Touch channel 10 is GPIO10(ESP32-S2) */
    TOUCH_PAD_NUM11,    /*!< Touch channel 11 is GPIO11(ESP32-S2) */
    TOUCH_PAD_NUM12,    /*!< Touch channel 12 is GPIO12(ESP32-S2) */
    TOUCH_PAD_NUM13,    /*!< Touch channel 13 is GPIO13(ESP32-S2) */
    TOUCH_PAD_NUM14,    /*!< Touch channel 14 is GPIO14(ESP32-S2) */
#endif
    TOUCH_PAD_MAX,
} touch_pad_t;

#endif