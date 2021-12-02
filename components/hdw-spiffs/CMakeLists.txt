idf_component_register(SRCS "pngle/pngle.c" "pngle/miniz.c" "spiffs_manager.c"
                    INCLUDE_DIRS "." "./pngle"  "../hdw-tft"
                    REQUIRES "spiffs")