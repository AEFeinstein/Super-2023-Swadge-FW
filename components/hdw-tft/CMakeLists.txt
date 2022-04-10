idf_component_register(SRCS "hdw-tft.c"
                    INCLUDE_DIRS "." "../hdw-spiffs/"
                    REQUIRES "esp_lcd" "bootloader_support")
