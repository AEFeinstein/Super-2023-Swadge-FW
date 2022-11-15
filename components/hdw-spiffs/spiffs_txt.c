#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_log.h"
#include "spiffs_manager.h"
#include "spiffs_txt.h"

/**
 * @brief Load a TXT from ROM to RAM. TXTs placed in the spiffs_image folder
 * before compilation will be automatically flashed to ROM
 *
 * @param name The filename of the TXT to load
 * @return A pointer to a null terminated TXT string. May be NULL if the load
 *         fails. Must be freed after use
 */
char* loadTxt(const char* name)
{
    // Read TXT from file
    uint8_t* buf = NULL;
    size_t sz;
    if(!spiffsReadFile(name, &buf, &sz, true))
    {
        ESP_LOGE("TXT", "Failed to read %s", name);
		return NULL;
    }

    return (char*)buf;
}

/**
 * @brief Free an allocated TXT string
 *
 * @param txtStr the TXT string to free
 */
void freeTxt(char* txtStr)
{
    free(txtStr);
}
