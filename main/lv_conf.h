#ifndef LV_CONF_H
#define LV_CONF_H

/* Set this to 1 to skip including lv_conf.h and use Kconfig settings */
#define LV_CONF_SKIP 0

/* Enable font cache */
#define LV_USE_FONT_CACHE 1
#define LV_FONT_CACHE_SIZE (128 * 1024)  /* 增加到128KB字体缓存，为中文字符提供更多空间 */

/* Enable image cache */
#define LV_USE_IMG_CACHE 1
#define LV_IMG_CACHE_DEF_SIZE 128  /* 增加图像缓存大小 */

/* Enable FreeType cache */
#define LV_USE_FREETYPE 1
#define LV_FREETYPE_CACHE_SIZE 32  /* 增加FreeType缓存大小 */
#define LV_FREETYPE_MAX_FACES 16   /* 增加最大字体面数 */
#define LV_FREETYPE_MAX_SIZES 16   /* 增加最大字体尺寸数 */
#define LV_FREETYPE_MAX_BYTES 8000000  /* 增加最大字节数 */

/* 启用更快的刷新率 */
#define LV_DISP_DEF_REFR_PERIOD 20  /* 默认刷新周期20ms (50Hz) */
#define LV_INDEV_DEF_READ_PERIOD 10 /* 输入设备读取周期10ms */

/* Enable GPU acceleration */
#define LV_USE_GPU_NXP_PXP 1
#define LV_USE_GPU_NXP_PXP_AUTO_INIT 1
#define LV_USE_GPU_ESP_PXP 1
#define LV_USE_GPU_ESP_DMA2D 1
#define LV_USE_GPU_ESP_JPEG 1
#define LV_USE_GPU_ARM2D 1
#define LV_USE_GPU_SDL 1

/* Enable vector graphics */
#define LV_USE_VECTOR_GRAPHIC_SKIA 1
#define LV_VECTOR_GRAPHIC_SKIA_PATH "/path/to/skia"
#define LV_USE_VECTOR_GRAPHIC_AGG 1
#define LV_USE_VECTOR_GRAPHIC_NANOVG 1

/* Enable assertions */
#define LV_USE_ASSERT_MEM 1
#define LV_USE_ASSERT_STR 1
#define LV_USE_ASSERT_OBJ_CHECK 1

#endif /* LV_CONF_H */