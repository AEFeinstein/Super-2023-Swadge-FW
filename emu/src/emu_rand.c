#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "esp_random.h"
#include "esp_log.h"

#include "emu_esp.h"

/**
 * @brief  Get one random 32-bit word from hardware RNG
 *
 * @return Random value between 0 and UINT32_MAX
 */
uint32_t esp_random(void)
{
    static bool seeded = false;
    if (!seeded)
    {
        seeded = true;
        srand(time(NULL));
    }
    return rand();
}
