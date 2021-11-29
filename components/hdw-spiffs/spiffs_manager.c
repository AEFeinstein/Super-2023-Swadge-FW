#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "spiffs_manager.h"

#define SPIFFS_DEBUG_PRINT
#ifdef SPIFFS_DEBUG_PRINT
#define spiffs_printf(...) printf(__VA_ARGS__)
#else
#define spiffs_printf(...)
#endif

/* Config data */
static const esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = false
};

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool initSpiffs(void)
{
    /* Initialize SPIFFS
     * Use settings defined above to initialize and mount SPIFFS filesystem.
     * Note: esp_vfs_spiffs_register is an all-in-one convenience function.
     */
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            spiffs_printf("Failed to mount or format filesystem\n");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            spiffs_printf("Failed to find SPIFFS partition\n");
        } else {
            spiffs_printf("Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
        }
        return false;
    }

#ifdef SPIFFS_DEBUG_PRINT
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        spiffs_printf("Failed to get SPIFFS partition information (%s)\n", esp_err_to_name(ret));
        return false;
    } else {
        spiffs_printf("Partition size: total: %d, used: %d\n", total, used);
    }
#endif

    return true;
}

/**
 * @brief TODO
 * 
 * @return true 
 * @return false 
 */
bool deinitSpiffs(void)
{
    return (ESP_OK == esp_vfs_spiffs_unregister(conf.partition_label));
}

/**
 * The following calls demonstrate reading files from the generated SPIFFS
 * image. The images should contain the same files and contents as the
 * spiffs_image directory.
 * 
 * @return true 
 * @return false 
 */
bool spiffsReadFile(char * fname)
{
    // Read and display the contents of a small text file
    spiffs_printf("Reading %s\n", fname);

    // Open for reading the given file
    char fnameFull[128] = "/spiffs/";
    strcat(fnameFull, fname);
    FILE* f = fopen(fnameFull, "r");
    if (f == NULL) {
        spiffs_printf("Failed to open %s\n", fnameFull);
        return false;
    }

    // Get the file size
    fseek(f, 0L, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0L, SEEK_SET);

    // Read the file into an array
    char buf[sz + 1];
    memset(buf, 0, sizeof(buf));
    fread(buf, sz, 1, f);
    buf[sz] = 0;

    // Close the file
    fclose(f);

    // Display the read contents from the file
    spiffs_printf("Read from %s: %ld bytes: %s\n", fname, sz, buf);
    return true;
}

bool spiffsTest(void)
{
    spiffsReadFile("text.txt");
    spiffsReadFile("autogen.txt");
    return true;
}
