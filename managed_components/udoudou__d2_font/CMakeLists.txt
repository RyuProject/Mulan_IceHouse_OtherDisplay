idf_component_register(SRCS "d2_font_fmt_txt.c" "d2_font.c"
                       INCLUDE_DIRS "include"
                       REQUIRES lvgl
                       PRIV_REQUIRES esp_partition mbedtls)
