#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "nvs_manager.h"

#define PARTITION_NAME "storage"

/**
 * @brief TODO
 *
 * @return true
 * @return false
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
 * @brief TODO
 *
 * @param key
 * @param val
 * @return true
 * @return false
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
                    printf("%s err %s\n", __func__, esp_err_to_name(writeErr));
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
            printf("%s openErr %s\n", __func__, esp_err_to_name(openErr));
            return false;
        }
    }
}

/**
 * @brief TODO
 *
 * @param key
 * @param outVal
 * @return true
 * @return false
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
                    printf("%s readErr %s\n", __func__, esp_err_to_name(readErr));
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
            printf("%s openErr %s\n", __func__, esp_err_to_name(openErr));
            return false;
        }
    }
}
