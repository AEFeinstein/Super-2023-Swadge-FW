#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_spiffs.h"
#include "spiffs_manager.h"
#include "upng.h"
#include "spiffs_config.h"

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
bool spiffsReadFile(const char * fname, uint8_t ** output, size_t * outsize)
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

    return true;
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
    // Concatenate the file name
    char fnameFull[128] = "/spiffs/";
    strcat(fnameFull, name);

    // Read the PNG from the file
    upng_t * png = upng_new_from_file(fnameFull);
    upng_decode(png);

    // Local variables for PNG metadata
    unsigned int width  = upng_get_width(png);
    unsigned int height = upng_get_height(png);
    unsigned int depth  = upng_get_bpp(png) / 8;
    upng_format format  = upng_get_format(png);

    // Validate the format
    if(UPNG_RGB8 != format && UPNG_RGBA8 != format)
    {
        ESP_LOGE("SPIFFS", "Invalid PNG format %s", name);
        return false;
    }

    // Return data through parameters
    *w = width;
    *h = height;
    (*pxOut) = (rgba_pixel_t*)malloc(sizeof(rgba_pixel_t) * width * height);

    // Convert the PNG to 565 color
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            (*pxOut)[(y * width) + x].rgb.c.r = (127 + (upng_get_buffer(png)[depth * ((y * width) + x) + 0]) * 31) / 255;
            (*pxOut)[(y * width) + x].rgb.c.g = (127 + (upng_get_buffer(png)[depth * ((y * width) + x) + 1]) * 63) / 255;
            (*pxOut)[(y * width) + x].rgb.c.b = (127 + (upng_get_buffer(png)[depth * ((y * width) + x) + 2]) * 31) / 255;
            if(UPNG_RGBA8 == format)
            {
                (*pxOut)[(y * width) + x].a = upng_get_buffer(png)[depth * ((y * width) + x) + 3];
            }
        }
    }

    // Free the upng data
    upng_free(png);

    // All done
    return true;
}

/**
 * @brief TODO
 *
 * @param name
 * @param font
 * @return true
 * @return false
 */
bool loadFont(const char * name, font_t * font)
{
    // Read font from file
    uint8_t * buf = NULL;
    size_t bufIdx = 0;
    size_t sz;
    spiffsReadFile(name, &buf, &sz);

    // Read the data into a font struct
    font->h = buf[bufIdx++];

    // Read each char
    for(char ch = ' '; ch <= '~'; ch++)
    {
        // Get an easy refence to this character
        font_ch_t * this = &font->chars[ch - ' '];

        // Read the width
        this->w = buf[bufIdx++];

        // Figure out what size the char is
        int pixels = font->h * this->w;
        int bytes = (pixels / 8) + ((pixels % 8 == 0) ? 0 : 1);

        // Allocate space for this char and copy it over
        this->bitmap = (uint8_t *) malloc(sizeof(uint8_t) * bytes);
        memcpy(this->bitmap, &buf[bufIdx], bytes);
        bufIdx += bytes;
    }

    // Free the SPIFFS data
    free(buf);

    return true;
}

/**
 * @brief TODO
 *
 * @param font
 */
void freeFont(font_t * font)
{
    for(char ch = ' '; ch <= '~'; ch++)
    {
        free(font->chars[ch - ' '].bitmap);
    }
}
