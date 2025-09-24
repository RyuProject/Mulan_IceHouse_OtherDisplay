/*
 * SPDX-FileCopyrightText: 2025 udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/** This describes a glyph.*/
typedef struct {
    uint16_t adv_w;                 /**< Draw the next glyph after this width. 12.4 format (real_value * 16 is stored).*/
    uint8_t box_w;                  /**< Width of the glyph's bounding box*/
    uint8_t box_h;                  /**< Height of the glyph's bounding box*/
    int8_t ofs_x;                   /**< x offset of the bounding box*/
    int8_t ofs_y;                   /**< y offset of the bounding box. Measured from the top of the line*/
} __attribute__((packed)) d2_font_fmt_txt_glyph_dsc_t;

typedef struct {
    uint32_t bitmap_index_offset : 21;     /**< index offset of the bitmap.*/
    uint32_t dsc_index : 11;
} __attribute__((packed))d2_font_fmt_txt_glyph_index_t;

/** Format of font character map.*/
typedef enum {
    D2_FONT_FMT_TXT_CMAP_FORMAT0_FULL,
    D2_FONT_FMT_TXT_CMAP_SPARSE_FULL,
    D2_FONT_FMT_TXT_CMAP_FORMAT0_TINY,
    D2_FONT_FMT_TXT_CMAP_SPARSE_TINY,
} d2_font_fmt_txt_cmap_type_t;

/**
 * Map codepoints to a `glyph_dsc`s
 * Several formats are supported to optimize memory usage
 * See https://github.com/lvgl/lv_font_conv/blob/master/doc/font_spec.md
 */
typedef struct {
    /** First Unicode character for this range*/
    uint32_t range_start;

    /** Number of Unicode characters related to this range.
     * Last Unicode character = range_start + range_length - 1*/
    uint16_t range_length;

    /** First glyph ID (array index of `glyph_dsc`) for this range*/
    uint16_t glyph_id_start;

    /** index base of the bitmap for this range*/
    uint32_t glyph_bitmap_index_base : 30;

    /** Type of this character map*/
    d2_font_fmt_txt_cmap_type_t type : 2;

    /*
    According the specification there are 4 formats:
        https://github.com/lvgl/lv_font_conv/blob/master/doc/font_spec.md

    For simplicity introduce "relative code point":
        rcp = codepoint - range_start

    and a search function:
        search a "value" in an "array" and returns the index of "value".

    Format 0 tiny
        unicode_list == NULL && glyph_id_ofs_list == NULL
        glyph_id = glyph_id_start + rcp

    Format 0 full
        unicode_list == NULL && glyph_id_ofs_list != NULL
        glyph_id = glyph_id_start + glyph_id_ofs_list[rcp]

    Sparse tiny
        unicode_list != NULL && glyph_id_ofs_list == NULL
        glyph_id = glyph_id_start + search(unicode_list, rcp)

    Sparse full
        unicode_list != NULL && glyph_id_ofs_list != NULL
        glyph_id = glyph_id_start + glyph_id_ofs_list[search(unicode_list, rcp)]
    */

    const uint16_t * unicode_list;

    /** if(type == D2_FONT_FMT_TXT_CMAP_FORMAT0_...) it's `uint8_t *`
     * if(type == D2_FONT_FMT_TXT_CMAP_SPARSE_...)  it's `uint16_t *`
     */
    const void * glyph_id_ofs_list;

    /** Length of `unicode_list` and/or `glyph_id_ofs_list`*/
    uint16_t list_length;

} __attribute__((packed)) d2_font_fmt_txt_cmap_t;

/** A simple mapping of kern values from pairs*/
typedef struct {
    /*To get a kern value of two code points:
       1. Get the `glyph_id_left` and `glyph_id_right` from `lv_font_fmt_txt_cmap_t
       2. for(i = 0; i < pair_cnt * 2; i += 2)
             if(glyph_ids[i] == glyph_id_left &&
                glyph_ids[i+1] == glyph_id_right)
                 return values[i / 2];
     */
    uint32_t pair_cnt   : 30;
    uint32_t glyph_ids_size : 2;    /**< 0: `glyph_ids` is stored as `uint8_t`; 1: as `uint16_t` */
    uint32_t glyph_id_max;          /**< The maximum value of `glyph_id_left` and `glyph_id_right`*/
    int8_t values[0];
    // void glyph_ids[0];
} __attribute__((packed))d2_font_fmt_txt_kern_pair_t;

/** Bitmap formats*/
typedef enum {
    D2_FONT_FMT_TXT_PLAIN      = 0,
    D2_FONT_FMT_TXT_COMPRESSED = 1,
    D2_FONT_FMT_TXT_COMPRESSED_NO_PREFILTER = 2,
} d2_font_fmt_txt_bitmap_format_t;

/** Describe store for additional data for fonts */
typedef struct {
    /** The bitmaps of all glyphs */
    const uint8_t * glyph_bitmap;

    /** Describe the glyphs */
    const d2_font_fmt_txt_glyph_index_t *glyph_index;
    const d2_font_fmt_txt_glyph_dsc_t * glyph_dsc;

    /** Map the glyphs to Unicode characters.
     *Array of `lv_font_cmap_fmt_txt_t` variables */
    const d2_font_fmt_txt_cmap_t * cmaps;

    /**
     * Store kerning values. `d2_font_fmt_txt_kern_pair_t *
     */
    const d2_font_fmt_txt_kern_pair_t * kern_dsc;

    /** Scale kern values in 12.4 format */
    uint16_t kern_scale;

    /** Number of cmap tables */
    uint16_t cmap_num       : 9;

    /** Bit per pixel: 1, 2, 3, 4, 8 */
    uint16_t bpp            : 4;

    /** Type of `kern_dsc` */
    uint16_t kern_classes   : 1;

    /**
     * storage format of the bitmap
     * from `d2_font_fmt_txt_bitmap_format_t`
     */
    d2_font_fmt_txt_bitmap_format_t bitmap_format  : 2;

} __attribute__((packed)) d2_font_fmt_txt_dsc_t;

typedef struct {
    uint32_t unicode_letter;
    uint32_t glyph_id;
    const d2_font_fmt_txt_cmap_t *cmap;
} d2_font_fmt_txt_glyph_cache_t;

typedef struct {
    void *base_ptr;
    void *mmap_handle;
    d2_font_fmt_txt_glyph_cache_t cache[2];
} d2_font_context_t;

#if LVGL_VERSION_MAJOR >= 9
/**
 * Used as `get_glyph_bitmap` callback in lvgl's native font format if the font is uncompressed.
 * @param g_dsc         the glyph descriptor including which font to use, which supply the glyph_index and format.
 * @param draw_buf      a draw buffer that can be used to store the bitmap of the glyph, it's OK not to use it.
 * @return pointer to an A8 bitmap (not necessarily bitmap_out) or NULL if `unicode_letter` not found
 */
const void *d2_font_get_bitmap_fmt_txt(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf);
#else
/**
 * Used as `get_glyph_bitmap` callback in LittelvGL's native font format if the font is uncompressed.
 * @param font pointer to font
 * @param unicode_letter a unicode letter which bitmap should be get
 * @return pointer to the bitmap or NULL if not found
 */
const uint8_t * d2_font_get_bitmap_fmt_txt(const lv_font_t * font, uint32_t letter);
#endif
/**
 * Used as `get_glyph_dsc` callback in lvgl's native font format if the font is uncompressed.
 * @param font pointer to font
 * @param dsc_out store the result descriptor here
 * @param unicode_letter a UNICODE letter code
 * @param unicode_letter_next the unicode letter succeeding the letter under test
 * @return true: descriptor is successfully loaded into `dsc_out`.
 *         false: the letter was not found, no data is loaded to `dsc_out`
 */
bool d2_font_get_glyph_dsc_fmt_txt(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode_letter,
                                   uint32_t unicode_letter_next);

#ifdef __cplusplus
} /*extern "C"*/
#endif
