#ifndef _TIMER_TYPES_H_
#define _TIMER_TYPES_H_

/**
 * @brief Selects a Timer-Group out of 2 available groups
 */
typedef enum {
    TIMER_GROUP_0 = 0, /*!<Hw timer group 0*/
#if SOC_TIMER_GROUPS > 1
    TIMER_GROUP_1 = 1, /*!<Hw timer group 1*/
#endif
    TIMER_GROUP_MAX,
} timer_group_t;

/**
 * @brief Select a hardware timer from timer groups
 */
typedef enum {
    TIMER_0 = 0, /*!<Select timer0 of GROUPx*/
#if SOC_TIMER_GROUP_TIMERS_PER_GROUP > 1
    TIMER_1 = 1, /*!<Select timer1 of GROUPx*/
#endif
    TIMER_MAX,
} timer_idx_t;

#endif
