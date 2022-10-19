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
// Function Prototypes
//==============================================================================

char* blobToStr(const void * value, size_t length);
int hexCharToInt(char c);
void strToBlob(char * str, void * outBlob, size_t blobLen);

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

/**
 * @brief Read a blob from NVS with a given string key
 *
 * @param key The key for the value to read
 * @param out_value The value will be written to this memory. It must be allocated before calling readNvsBlob()
 * @param length If out_value is `NULL`, this will be set to the length of the given key. Otherwise, it is the length of the blob to read.
 * @return true if the value was read, false if it was not
 */
bool readNvsBlob(const char* key, void* out_value, size_t* length)
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
                        char* strBlob = cJSON_GetStringValue(jsonIter);

                        if (out_value != NULL)
                        {
                            // The call to read, using returned length
                            strToBlob(strBlob, out_value, *length);
                        }
                        else
                        {
                            // The call to get length of blob
                            *length = strlen(strBlob) / 2;
                        }
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

/**
 * @brief Write a blob to NVS with a given string key
 *
 * @param key The key for the value to write
 * @param value The blob value to write
 * @param length The length of the blob
 * @return true if the value was written, false if it was not
 */
bool writeNvsBlob(const char* key, const void* value, size_t length)
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
            char * blobStr = blobToStr(value, length);
            cJSON * jsonVal = cJSON_CreateString(blobStr);
            free(blobStr);
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
 * @brief Delete the value with the given key from NVS
 *
 * @param key The NVS key to be deleted
 * @return true if the value was deleted, false if it was not
 */
bool eraseNvsKey(const char* key)
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

            // Check if the key exists
            cJSON * jsonIter;
            bool keyExists = false;
            cJSON_ArrayForEach(jsonIter, json)
            {
                if(0 == strcmp(jsonIter->string, key))
                {
                    keyExists = true;
                }
            }

            // Remove the key if it exists
            if(keyExists)
            {
                cJSON_DeleteItemFromObject(json, key);
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
 * @brief Convert a blob to a hex string
 * 
 * @param value The blob
 * @param length The length of the blob
 * @return char* An allocated hex string, must be free()'d when done
 */
char* blobToStr(const void * value, size_t length)
{
    const uint8_t * value8 = (const uint8_t *)value;
    char * blobStr = malloc((length * 2) + 1);
    for(size_t i = 0; i < length; i++)
    {
        sprintf(&blobStr[i*2], "%02X", value8[i]);
    }
    return blobStr;
}

/**
 * @brief Convert a hex char to an int
 * 
 * @param c A hex char, [0-9a-fA-F]
 * @return int The integer value for this char
 */
int hexCharToInt(char c)
{
    if('0' <= c && c <= '9')
    {
        return c - '0';
    }
    else if ('a' <= c && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    else if ('A' <= c && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return 0;
}

/**
 * @brief Convert a string to a blob
 * 
 * @param str A null terminated string
 * @param outBlob The blob will be written here, must already be allocated
 * @param blobLen The length of the blob to write
 */
void strToBlob(char * str, void * outBlob, size_t blobLen)
{
    uint8_t * outBlob8 = (uint8_t*)outBlob;
    for(size_t i = 0; i < blobLen; i++)
    {
        if(((2 * i) + 1) < strlen(str))
        {
            uint8_t upperNib = hexCharToInt(str[2 * i]);
            uint8_t lowerNib = hexCharToInt(str[(2 * i) + 1]);
            outBlob8[i] = (upperNib << 4) | (lowerNib);
        }
        else
        {
            outBlob8[i] = 0;
        }
    }
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
 * @param readToSpiRam unused
 * @return true if the file was read successfully, false otherwise
 */
bool spiffsReadFile(const char * fname, uint8_t ** output, size_t * outsize, bool readToSpiRam)
{
    // Make sure the output pointer is NULL to begin with
    if(NULL != *output)
    {
        // ESP_LOGE("SPIFFS", "output not NULL");
        return false;
    }

    // Read and display the contents of a small text file
    // ESP_LOGD("SPIFFS", "Reading %s", fname);

    // Open for reading the given file
    char fnameFull[128] = "./spiffs_image/";
    strcat(fnameFull, fname);
    FILE* f = fopen(fnameFull, "rb");
    if (f == NULL) {
        // ESP_LOGE("SPIFFS", "Failed to open %s", fnameFull);
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
    // ESP_LOGD("SPIFFS", "Read from %s: %d bytes", fname, (uint32_t)(*outsize));
    return true;
}
