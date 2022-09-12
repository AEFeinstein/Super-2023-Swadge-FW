//==============================================================================
// Includes
//==============================================================================

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "nvs_manager.h"

//==============================================================================
// Defines
//==============================================================================

#define PARTITION_NAME "storage"

//==============================================================================
// Functions
//==============================================================================

/**
 * @brief Initialize the nonvolatile storage
 *
 * @param firstTry true if this is the first time NVS is initialized this boot,
 *                 false otherwise
 * @return true if NVS was initialized and can be used, false if it failed
 */
bool initNvs(bool firstTry)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    // Check error
    switch(err)
    {
        case ESP_OK:
        {
            // NVS opened
            return true;
        }
        case ESP_ERR_NVS_NO_FREE_PAGES:
        case ESP_ERR_NVS_NEW_VERSION_FOUND:
        {
            // If this is the first try
            if(true == firstTry)
            {
                // NVS partition was truncated and needs to be erased
                // Retry nvs_flash_init
                switch(nvs_flash_erase())
                {
                    case ESP_OK:
                    {
                        // NVS erased, try initializing one more time
                        return initNvs(false);
                    }
                    default:
                    case ESP_ERR_NOT_FOUND:
                    {
                        // Couldn't erase flash
                        return false;
                    }
                }
            }
            else
            {
                // Failed twice, return false
                return false;
            }
            break;
        }
        default:
        case ESP_ERR_NOT_FOUND:
        case ESP_ERR_NO_MEM:
        {
            // NVS not opened
            return false;
        }
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
    nvs_handle_t handle;
    esp_err_t openErr = nvs_open(PARTITION_NAME, NVS_READWRITE, &handle);
    switch(openErr)
    {
        case ESP_OK:
        {
            // Assume the commit is bad
            bool commitOk = false;
            // Write the NVS
            esp_err_t writeErr = nvs_set_i32(handle, key, val);
            // Check the write error
            switch(writeErr)
            {
                case ESP_OK:
                {
                    // Commit NVS
                    commitOk = (ESP_OK == nvs_commit(handle));
                    break;
                }
                default:
                case ESP_ERR_NVS_INVALID_HANDLE:
                case ESP_ERR_NVS_READ_ONLY:
                case ESP_ERR_NVS_INVALID_NAME:
                case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
                case ESP_ERR_NVS_REMOVE_FAILED:
                {
                    ESP_LOGE("NVS", "%s err %s", __func__, esp_err_to_name(writeErr));
                    break;
                }
            }

            // Close the handle
            nvs_close(handle);
            return commitOk;
        }
        default:
        case ESP_ERR_NVS_NOT_INITIALIZED:
        case ESP_ERR_NVS_PART_NOT_FOUND:
        case ESP_ERR_NVS_NOT_FOUND:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NO_MEM:
        {
            ESP_LOGE("NVS", "%s openErr %s", __func__, esp_err_to_name(openErr));
            return false;
        }
    }
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
    nvs_handle_t handle;
    esp_err_t openErr = nvs_open(PARTITION_NAME, NVS_READONLY, &handle);
    switch(openErr)
    {
        case ESP_OK:
        {
            // Assume the commit is bad
            bool readOk = false;
            // Write the NVS
            esp_err_t readErr = nvs_get_i32(handle, key, outVal);
            // Check the write error
            switch(readErr)
            {
                case ESP_OK:
                {
                    readOk = true;
                    break;
                }
                default:
                case ESP_ERR_NVS_NOT_FOUND: // This is the error when a nonexistent key is read
                case ESP_ERR_NVS_INVALID_HANDLE:
                case ESP_ERR_NVS_INVALID_NAME:
                case ESP_ERR_NVS_INVALID_LENGTH:
                {
                    ESP_LOGE("NVS", "%s readErr %s", __func__, esp_err_to_name(readErr));
                    break;
                }
            }
            // Close the handle
            nvs_close(handle);
            return readOk;
        }
        default:
        case ESP_ERR_NVS_NOT_INITIALIZED:
        case ESP_ERR_NVS_PART_NOT_FOUND:
        case ESP_ERR_NVS_NOT_FOUND:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NO_MEM:
        {
            ESP_LOGE("NVS", "%s openErr %s", __func__, esp_err_to_name(openErr));
            return false;
        }
    }
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
    nvs_handle_t handle;
    esp_err_t openErr = nvs_open(PARTITION_NAME, NVS_READWRITE, &handle);
    switch(openErr)
    {
        case ESP_OK:
        {
            // Assume the commit is bad
            bool commitOk = false;
            // Write the NVS
            esp_err_t writeErr = nvs_set_blob(handle, key, value, length);
            // Check the write error
            switch(writeErr)
            {
                case ESP_OK:
                {
                    // Commit NVS
                    commitOk = (ESP_OK == nvs_commit(handle));
                    break;
                }
                default:
                case ESP_ERR_NVS_INVALID_HANDLE:
                case ESP_ERR_NVS_READ_ONLY:
                case ESP_ERR_NVS_INVALID_NAME:
                case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
                case ESP_ERR_NVS_REMOVE_FAILED:
                {
                    ESP_LOGE("NVS", "%s err %s", __func__, esp_err_to_name(writeErr));
                    break;
                }
            }

            // Close the handle
            nvs_close(handle);
            return commitOk;
        }
        default:
        case ESP_ERR_NVS_NOT_INITIALIZED:
        case ESP_ERR_NVS_PART_NOT_FOUND:
        case ESP_ERR_NVS_NOT_FOUND:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NO_MEM:
        {
            ESP_LOGE("NVS", "%s openErr %s", __func__, esp_err_to_name(openErr));
            return false;
        }
    }
}

/**
 * @brief Read a blob from NVS with a given string key
 * 
 * @param key The key for the value to read
 * @param out_value The value will be written to this memory. It must be allocated before calling readNvsBlob()
 * @param length The length of the value that was read
 * @return true if the value was read, false if it was not
 */
bool readNvsBlob(const char* key, void* out_value, size_t* length)
{
    nvs_handle_t handle;
    esp_err_t openErr = nvs_open(PARTITION_NAME, NVS_READONLY, &handle);
    switch(openErr)
    {
        case ESP_OK:
        {
            // Assume the commit is bad
            bool readOk = false;
            // Write the NVS
            esp_err_t readErr = nvs_get_blob(handle, key, out_value, length);
            // Check the write error
            switch(readErr)
            {
                case ESP_OK:
                {
                    readOk = true;
                    break;
                }
                default:
                case ESP_ERR_NVS_NOT_FOUND: // This is the error when a nonexistent key is read
                case ESP_ERR_NVS_INVALID_HANDLE:
                case ESP_ERR_NVS_INVALID_NAME:
                case ESP_ERR_NVS_INVALID_LENGTH:
                {
                    ESP_LOGE("NVS", "%s readErr %s", __func__, esp_err_to_name(readErr));
                    break;
                }
            }
            // Close the handle
            nvs_close(handle);
            return readOk;
        }
        default:
        case ESP_ERR_NVS_NOT_INITIALIZED:
        case ESP_ERR_NVS_PART_NOT_FOUND:
        case ESP_ERR_NVS_NOT_FOUND:
        case ESP_ERR_NVS_INVALID_NAME:
        case ESP_ERR_NO_MEM:
        {
            ESP_LOGE("NVS", "%s openErr %s", __func__, esp_err_to_name(openErr));
            return false;
        }
    }
}
