# D2_font

[![Component Registry](https://components.espressif.com/components/udoudou/d2_font/badge.svg)](https://components.espressif.com/components/udoudou/d2_font)

This component provides a new font engine for LVGL. Compared to LVGL's internal engine, this engine allows font data to be kept separate from the application. By leveraging ESP-IDF's MMAP feature, loading fonts requires only a small amount of memory resources.

This component heavily references the LVGL font engine implementation. Font-related structures have been adjusted and optimized to accommodate direct access to font data using MMAP.

Thanks to optimized data structures, even when font data is compiled directly into an application, with a large number of characters, the font data can still occupy a considerable amount less space than the font data of the LVGL native font engine with the same font configuration (the performance of character search indexing may be slightly reduced). This is particularly suitable for Chinese fonts. For example, the same font generation parameters `--bpp 1 --size 14 --no-compress --stride 1 --align 1 --font AlibabaPuHuiTi-3-55-Regular.ttf --range 0x0020-0x007F,0x0080-0x00BF,0x2600-0x26FF,0x3000-0x301F,0x4E00-0x9FAF,0xFF00-0xFF64` get the Chinese font data, compiled firmware size:

 - No Chinese fonts used: 462593 bytes
 - Use [D2_font](../../examples/d2_font/main/d2_font_demo_14.c): 968413 bytes (505820 bytes)
 - Use [LVGL](../../examples/d2_font/main/lvgl_font_demo_14.c): 1051497 bytes (588904 bytes)

## Font data

Font data is available in two formats:
 - [.c](../../examples/d2_font/main/d2_font_demo_14.c)
    - Directly participate in compilation and merge into the application.
 - [.bin](../../examples/d2_font/main/d2_font_demo_14.bin)
    - Separate font data. Can be burned directly into a separate partition and loaded directly via `d2_font_load_from_partition`.
    - Alternatively, you can use other data storage systems to store bin files. For example, [esp_mmap_assets](https://components.espressif.com/components/espressif/esp_mmap_assets) , a simple data indexing structure, packages multiple font bins into the same partition, providing the mmap access address and size for each file. Alternatively, you can use the file system to read the bin file into memory (this will consume the same amount of memory as the font bin size, which was not originally intended for this component). The font can then be loaded using `d2_font_load_from_mem`.

## Adding a New Font

There are several ways to add a new font to your project:

 - The simplest method is to use the [Online font converter](https://udoudou.github.io/lv_font_conv/). Just set the parameters, click the Convert button, copy the font to your project and use it. Be sure to carefully read the steps provided on that site or you will get an error while converting.

 - Use the [Offline font converter](https://github.com/udoudou/lv_font_conv). (Requires Node.js to be installed)
