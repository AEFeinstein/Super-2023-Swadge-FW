idf_component_register(SRCS "crashwrap.c" INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash )

#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=esp_panic_handler" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=panic_abort" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=ieee80211_ioctl" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=esp_wifi_internal_ioctl" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=nvs_set_u8" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=nvs_get_u8" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=nvs_set_i8" APPEND)
#idf_build_set_property(LINK_OPTIONS "-Wl,--wrap=nvs_open" APPEND)





