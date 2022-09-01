//==============================================================================
// Includes
//==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "cJSON.h"

#include "emu_esp.h"
#include "nvs_manager.h"
#include "spiffs_manager.h"

//==============================================================================
// Defines
//==============================================================================

#define NVS_JSON_FILE "nvs.json"

//==============================================================================
// NVS
//==============================================================================

/**
 * @brief Initialize NVS by making sure the file exists
 *
 * @param firstTry unused
 * @return true if the file exists or was created, false otherwise
 */
bool initNvs(bool firstTry UNUSED)
{
    // Check if the json file exists
    if( access( NVS_JSON_FILE, F_OK ) != 0 )
    {
        FILE * nvsFile = fopen(NVS_JSON_FILE, "wb");
        if(NULL != nvsFile)
        {
            if(1 == fwrite("{}", sizeof("{}"), 1, nvsFile))
            {
                // Wrote successfully
                fclose(nvsFile);
                return true;
            }
            else
            {
                // Failed to write
                fclose(nvsFile);
                return false;
            }
        }
        else
        {
            // Couldn't open file
            return false;
        }
    }
    else
    {
        // File exists
        return true;
    }
}

/**
 * @brief Write a 32 bit value to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param val The value to write
 * @return true if the value was written, false if it was not
 */
bool writeNvs32(const char* key, int32_t val)
{
    // Open the file
    FILE * nvsFile = fopen(NVS_JSON_FILE, "rb");
    if(NULL != nvsFile)
    {
        // Get the file size
        fseek(nvsFile, 0L, SEEK_END);
        size_t fsize = ftell(nvsFile);
        fseek(nvsFile, 0L, SEEK_SET);

        // Read the file
        char fbuf[fsize + 1];
        fbuf[fsize] = 0;
        if(fsize == fread(fbuf, 1, fsize, nvsFile))
        {
            // Close the file
            fclose(nvsFile);

            // Parse the JSON
            cJSON * json = cJSON_Parse(fbuf);

            // Check if the key alredy exists
            cJSON * jsonIter;
            bool keyExists = false;
            cJSON_ArrayForEach(jsonIter, json)
            {
                if(0 == strcmp(jsonIter->string, key))
                {
                    keyExists = true;
                }
            }

            // Add or replace the item
            cJSON * jsonVal = cJSON_CreateNumber(val);
            if(keyExists)
            {
                cJSON_ReplaceItemInObject(json, key, jsonVal);
            }
            else
            {
                cJSON_AddItemToObject(json, key, jsonVal);
            }

            // Write the new JSON back to the file
            FILE * nvsFileW = fopen(NVS_JSON_FILE, "wb");
            if(NULL != nvsFileW)
            {
                char * jsonStr = cJSON_Print(json);
                fprintf(nvsFileW, "%s", jsonStr);
                fclose(nvsFileW);

                free(jsonStr);
                cJSON_Delete(json);

                return true;
            }
            else
            {
                // Couldn't open file to write
            }
            cJSON_Delete(json);
        }
        else
        {
            // Couldn't read file
            fclose(nvsFile);
        }
    }
    else
    {
        // couldn't open file to read
    }
    return false;
}

/**
 * @brief Read a 32 bit value from NVS with a given string key
 *
 * @param key The key for the value to read
 * @param outVal The value that was read
 * @return true if the value was read, false if it was not
 */
bool readNvs32(const char* key, int32_t* outVal)
{
    // Open the file
    FILE * nvsFile = fopen(NVS_JSON_FILE, "rb");
    if(NULL != nvsFile)
    {
        // Get the file size
        fseek(nvsFile, 0L, SEEK_END);
        size_t fsize = ftell(nvsFile);
        fseek(nvsFile, 0L, SEEK_SET);

        // Read the file
        char fbuf[fsize + 1];
        fbuf[fsize] = 0;
        if(fsize == fread(fbuf, 1, fsize, nvsFile))
        {
            // Close the file
            fclose(nvsFile);

            // Parse the JSON
            cJSON * json = cJSON_Parse(fbuf);
            cJSON * jsonIter = json;

            // Find the requested key
            char *current_key = NULL;
            cJSON_ArrayForEach(jsonIter, json)
            {
                current_key = jsonIter->string;
                if (current_key != NULL)
                {
                    // If the key matches
                    if(0 == strcmp(current_key, key))
                    {
                        // Return the value
                        *outVal = (int32_t)cJSON_GetNumberValue(jsonIter);
                        cJSON_Delete(json);
                        return true;
                    }
                }
            }
            cJSON_Delete(json);
        }
        else
        {
            fclose(nvsFile);
        }
    }
    return false;
}

//==============================================================================
// SPIFFS
//==============================================================================

/**
 * @brief Do nothing, the normal file system replaces SPIFFS well
 *
 * @return true
 */
bool initSpiffs(void)
{
    return true;
}

/**
 * @brief Do nothing, the normal file system replaces SPIFFS well
 *
 * @return true
 */
bool deinitSpiffs(void)
{
    return false;
}

/**
 * @brief Read a file from SPIFFS into an output array. Files that are in the
 * spiffs_image folder before compilation and flashing will automatically
 * be included in the firmware
 *
 * @param fname   The name of the file to load
 * @param output  A pointer to a pointer to return the read data in. This memory
 *                will be allocated with calloc(). Must be NULL to start
 * @param outsize A pointer to a size_t to return how much data was read
 * @return true if the file was read successfully, false otherwise
 */
bool spiffsReadFile(const char * fname, uint8_t ** output, size_t * outsize)
{
    // Make sure the output pointer is NULL to begin with
    if(NULL != *output)
    {
        ESP_LOGE("SPIFFS", "output not NULL");
        return false;
    }

    // Read and display the contents of a small text file
    ESP_LOGD("SPIFFS", "Reading %s", fname);

    // Open for reading the given file
    char fnameFull[128] = "./spiffs_image/";
    strcat(fnameFull, fname);
    FILE* f = fopen(fnameFull, "rb");
    if (f == NULL) {
        ESP_LOGE("SPIFFS", "Failed to open %s", fnameFull);
        return false;
    }

    // Get the file size
    fseek(f, 0L, SEEK_END);
    *outsize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    // Read the file into an array
    *output = (uint8_t*)calloc((*outsize + 1), sizeof(uint8_t));
    fread(*output, *outsize, 1, f);

    // Close the file
    fclose(f);

    // Display the read contents from the file
    ESP_LOGD("SPIFFS", "Read from %s: %d bytes", fname, (uint32_t)(*outsize));
    return true;
}
