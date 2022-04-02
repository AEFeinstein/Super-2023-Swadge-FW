idf_component_register(SRCS "spiffs_manager.c" "spiffs_json.c" "heatshrink_decoder.c"
                    INCLUDE_DIRS "."  "../hdw-tft"
                    REQUIRES "spiffs")