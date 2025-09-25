/**
 * @file lv_conf.h
 * Configuration file for LVGL
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* Enable LVGL built-in memory management */
#define LV_MEM_CUSTOM 1
#define LV_MEM_SIZE (32 * 1024)  /* 32KB memory pool */

/* Enable font caching to improve performance */
#define LV_FONT_CACHE 1
#define LV_FONT_CACHE_SIZE 1024  /* 1KB font cache */

/* Enable built-in Montserrat font */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_38 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_42 1
#define LV_FONT_MONTSERRAT_44 1
#define LV_FONT_MONTSERRAT_46 1
#define LV_FONT_MONTSERRAT_48 1

/* Disable unused features to save memory */
#define LV_USE_ANIMATION 1
#define LV_USE_SHADOW 0
#define LV_USE_BLEND_MODES 0
#define LV_USE_OPA_SCALE 0
#define LV_USE_IMG_TRANSFORM 0
#define LV_USE_FILE_EXPLORER 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_TABLE 0
#define LV_USE_CALENDAR 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPINBOX 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_KEYBOARD 0
#define LV_USE_IMGBTN 0
#define LV_USE_SPAN 0
#define LV_USE_SPINNER 0

/* Optimize memory usage */
#define LV_MEMCPY_MEMSET_STD 1
#define LV_ATTRIBUTE_FAST_MEM 1
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY 1

/* Reduce color depth to save memory */
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Optimize draw buffer */
#define LV_DRAW_BUF_ALIGN 4
#define LV_DRAW_BUF_STRIDE_ALIGN 4

/* Enable important features */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0

/* Enable essential widgets */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1

/* Optimize style memory usage */
#define LV_STYLE_NUM_FLAGS 32
#define LV_STYLE_MAX_RECURSION_DEPTH 8

/* Enable Chinese font support if needed */
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_SUBPX 0

#endif /* LV_CONF_H */