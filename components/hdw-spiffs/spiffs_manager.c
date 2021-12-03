#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "spiffs_manager.h"
#include "pngle.h"

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
bool spiffsReadFile(char * fname, uint8_t ** output, size_t * outsize)
{
    if(NULL != *output)
    {
        spiffs_printf("output not NULL\n");
        return false;
    }

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
    *outsize = ftell(f);
    fseek(f, 0L, SEEK_SET);

    // Read the file into an array
    *output = (uint8_t*)calloc((*outsize + 1), sizeof(uint8_t));
    fread(*output, *outsize, 1, f);

    // Close the file
    fclose(f);

    // Display the read contents from the file
    spiffs_printf("Read from %s: %u bytes\n", fname, *outsize);
    return true;
}

bool spiffsTest(void)
{
    uint8_t * buf = NULL;
    size_t sz;

    spiffsReadFile("text.txt", &buf, &sz);
    free(buf);
    buf = NULL;

    spiffsReadFile("autogen.txt", &buf, &sz);
    free(buf);
    buf = NULL;

    return true;
}

rgba_pixel_t ** g_pxOut;
uint16_t * g_w;
uint16_t * g_h;

/**
 * @brief TODO
 * 
 * @param pngle 
 * @param x 
 * @param y 
 * @param w 
 * @param h 
 * @param rgba 
 */
void pngle_on_draw(pngle_t *pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4])
{
    if(NULL == *g_pxOut)
    {
        *g_w = pngle_get_width(pngle);
        *g_h = pngle_get_height(pngle);

        *g_pxOut = (rgba_pixel_t*)malloc(sizeof(rgba_pixel_t) * (*g_w) * (*g_h));
    }

    (*g_pxOut)[((y * (*g_w)) + x)].a = rgba[3];
    (*g_pxOut)[((y * (*g_w)) + x)].rgb.c.r = (127 + (rgba[2] * 31)) / 255;
    (*g_pxOut)[((y * (*g_w)) + x)].rgb.c.g = (127 + (rgba[1] * 63)) / 255;
    (*g_pxOut)[((y * (*g_w)) + x)].rgb.c.b = (127 + (rgba[0] * 31)) / 255;
}

/**
 * @brief
 *
 * @param name
 * @param pxOut
 * @param w
 * @param h
 * @return true
 * @return false
 */
bool loadPng(char * name, rgba_pixel_t ** pxOut, uint16_t * w, uint16_t * h)
{
    if(NULL != *pxOut)
    {
        return false;
    }

    // Read PNG from file
    uint8_t * buf = NULL;
    size_t sz;
    spiffsReadFile(name, &buf, &sz);

    // save global vars
    g_w = w;
    g_h = h;
    g_pxOut = pxOut;

    // Decode the PNG
    pngle_t *pngle = pngle_new();
    pngle_set_draw_callback(pngle, pngle_on_draw);
    pngle_feed(pngle, buf, sz);
    pngle_destroy(pngle);

    free(buf);

    return true;
}
