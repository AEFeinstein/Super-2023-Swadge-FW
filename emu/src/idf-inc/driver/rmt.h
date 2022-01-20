#ifndef _RMT_H_
#define _RMT_H_

typedef enum {
    RMT_CHANNEL_0,  /*!< RMT channel number 0 */
    RMT_CHANNEL_1,  /*!< RMT channel number 1 */
    RMT_CHANNEL_2,  /*!< RMT channel number 2 */
    RMT_CHANNEL_3,  /*!< RMT channel number 3 */
#if SOC_RMT_CHANNELS_PER_GROUP > 4
    RMT_CHANNEL_4,  /*!< RMT channel number 4 */
    RMT_CHANNEL_5,  /*!< RMT channel number 5 */
    RMT_CHANNEL_6,  /*!< RMT channel number 6 */
    RMT_CHANNEL_7,  /*!< RMT channel number 7 */
#endif
    RMT_CHANNEL_MAX /*!< Number of RMT channels */
} rmt_channel_t;

#endif