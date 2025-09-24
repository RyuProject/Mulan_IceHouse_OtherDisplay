/*
 * SPDX-FileCopyrightText: 2025 udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "d2_font_fmt_txt.h"

#include "src/misc/lv_utils.h"

#if LV_USE_FONT_COMPRESSED
typedef enum {
    D2_RLE_STATE_SINGLE = 0,
    D2_RLE_STATE_REPEATED,
    D2_RLE_STATE_COUNTER,
} d2_font_fmt_rle_state_t;

typedef struct {
    uint32_t rdp;
    const uint8_t * in;
    uint8_t bpp;
    uint8_t prev_v;
    uint8_t count;
    d2_font_fmt_rle_state_t state;
} d2_font_fmt_rle_t;
#endif /*LV_USE_FONT_COMPRESSED*/

typedef struct {
    uint32_t gid_left;
    uint32_t gid_right;
} kern_pair_ref_t;

static uint32_t get_glyph_dsc_id(const lv_font_t * font, uint32_t letter, const d2_font_fmt_txt_cmap_t **cmap);
static int8_t get_kern_value(const lv_font_t * font, uint32_t gid_left, uint32_t gid_right);
#if LVGL_VERSION_MAJOR >= 9
static int unicode_list_compare(const void * ref, const void * element);
static int kern_pair_8_compare(const void * ref, const void * element);
static int kern_pair_16_compare(const void * ref, const void * element);
#else
#define lv_utils_bsearch _lv_utils_bsearch
#define lv_uintptr_t uintptr_t
static int32_t unicode_list_compare(const void * ref, const void * element);
static int32_t kern_pair_8_compare(const void * ref, const void * element);
static int32_t kern_pair_16_compare(const void * ref, const void * element);
#endif

#if LV_USE_FONT_COMPRESSED
static void decompress(const uint8_t * in, uint8_t * out, int32_t w, int32_t h, uint8_t bpp, bool prefilter);
static inline void decompress_line(d2_font_fmt_rle_t *rle, uint8_t * out, int32_t w);
static inline void rle_init(d2_font_fmt_rle_t *rle, const uint8_t * in,  uint8_t bpp);
static inline uint8_t rle_next(d2_font_fmt_rle_t *rle);
static inline uint8_t get_bits(const uint8_t * in, uint32_t bit_pos, uint8_t len);
#if LVGL_VERSION_MAJOR < 9
static inline void bits_write(uint8_t * out, uint32_t bit_pos, uint8_t val, uint8_t len);
#endif

#endif /*LV_USE_FONT_COMPRESSED*/

#if LVGL_VERSION_MAJOR >= 9
static const uint8_t opa4_table[16] = {0,  17, 34,  51,
                                       68, 85, 102, 119,
                                       136, 153, 170, 187,
                                       204, 221, 238, 255
                                      };

#if LV_USE_FONT_COMPRESSED
static const uint8_t opa3_table[8] = {0, 36, 73, 109, 146, 182, 218, 255};
#endif

static const uint8_t opa2_table[4] = {0, 85, 170, 255};
#include <stdio.h>
const void *d2_font_get_bitmap_fmt_txt(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf)
#else
const uint8_t * d2_font_get_bitmap_fmt_txt(const lv_font_t * font, uint32_t letter)
#endif
{
#if LVGL_VERSION_MAJOR >= 9
    const lv_font_t *font = g_dsc->resolved_font;
#endif
    d2_font_context_t *ctx = (d2_font_context_t *)font->user_data;

    d2_font_fmt_txt_dsc_t * fdsc = (d2_font_fmt_txt_dsc_t *)(ctx->base_ptr + (uint32_t)font->dsc);
#if LVGL_VERSION_MAJOR >= 9
    uint32_t letter = g_dsc->gid.index;
#endif
    const d2_font_fmt_txt_cmap_t * cmap;
    uint32_t gid = get_glyph_dsc_id(font, letter, &cmap);
    if (!gid) {
        return NULL;
    }

    const d2_font_fmt_txt_glyph_index_t *gindex = (d2_font_fmt_txt_glyph_index_t *)(ctx->base_ptr + (uint32_t)fdsc->glyph_index) + gid;
    const d2_font_fmt_txt_glyph_dsc_t *gdsc = (d2_font_fmt_txt_glyph_dsc_t *)(ctx->base_ptr + (uint32_t)fdsc->glyph_dsc) + gindex->dsc_index;
    const uint8_t * bitmap_in = (uint8_t*)(ctx->base_ptr + (uint32_t)fdsc->glyph_bitmap) + cmap->glyph_bitmap_index_base + gindex->bitmap_index_offset;

#if LVGL_VERSION_MAJOR >= 10    //todo
    if (g_dsc->req_raw_bitmap) {
        return bitmap_in;
    }
#endif
#if LVGL_VERSION_MAJOR >= 9
    uint8_t * bitmap_out = draw_buf->data;
#endif
    int32_t gsize = (int32_t) gdsc->box_w * gdsc->box_h;
    if (gsize == 0) {
        return NULL;
    }

    if (fdsc->bitmap_format == D2_FONT_FMT_TXT_PLAIN) {
#if LVGL_VERSION_MAJOR >= 9
        uint8_t * bitmap_out_tmp = bitmap_out;
        int32_t i = 0;
        int32_t x, y;
        uint32_t stride_out = lv_draw_buf_width_to_stride(gdsc->box_w, LV_COLOR_FORMAT_A8);
        if (fdsc->bpp == 1) {
            for (y = 0; y < gdsc->box_h; y ++) {
                for (x = 0; x < gdsc->box_w; x++, i++) {
                    i = i & 0x7;
                    if (i == 0) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x80 ? 0xff : 0x00;
                    } else if (i == 1) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x40 ? 0xff : 0x00;
                    } else if (i == 2) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x20 ? 0xff : 0x00;
                    } else if (i == 3) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x10 ? 0xff : 0x00;
                    } else if (i == 4) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x08 ? 0xff : 0x00;
                    } else if (i == 5) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x04 ? 0xff : 0x00;
                    } else if (i == 6) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x02 ? 0xff : 0x00;
                    } else if (i == 7) {
                        bitmap_out_tmp[x] = (*bitmap_in) & 0x01 ? 0xff : 0x00;
                        bitmap_in++;
                    }
                }
                bitmap_out_tmp += stride_out;
            }
        } else if (fdsc->bpp == 2) {
            for (y = 0; y < gdsc->box_h; y ++) {
                for (x = 0; x < gdsc->box_w; x++, i++) {
                    i = i & 0x3;
                    if (i == 0) {
                        bitmap_out_tmp[x] = opa2_table[(*bitmap_in) >> 6];
                    } else if (i == 1) {
                        bitmap_out_tmp[x] = opa2_table[((*bitmap_in) >> 4) & 0x3];
                    } else if (i == 2) {
                        bitmap_out_tmp[x] = opa2_table[((*bitmap_in) >> 2) & 0x3];
                    } else if (i == 3) {
                        bitmap_out_tmp[x] = opa2_table[((*bitmap_in) >> 0) & 0x3];
                        bitmap_in++;
                    }
                }

                bitmap_out_tmp += stride_out;
            }

        } else if (fdsc->bpp == 4) {
            for (y = 0; y < gdsc->box_h; y ++) {
                for (x = 0; x < gdsc->box_w; x++, i++) {
                    i = i & 0x1;
                    if (i == 0) {
                        bitmap_out_tmp[x] = opa4_table[(*bitmap_in) >> 4];
                    } else if (i == 1) {
                        bitmap_out_tmp[x] = opa4_table[(*bitmap_in) & 0xF];
                        bitmap_in++;
                    }
                }
                bitmap_out_tmp += stride_out;
            }
        } else if (fdsc->bpp == 8) {
            for (y = 0; y < gdsc->box_h; y ++) {
                for (x = 0; x < gdsc->box_w; x++, i++) {
                    bitmap_out_tmp[x] = *bitmap_in;
                    bitmap_in++;
                }
                bitmap_out_tmp += stride_out;
            }
        }

        lv_draw_buf_flush_cache(draw_buf, NULL);
        return draw_buf;
#else
        return bitmap_in;
#endif
    }
    /*Handle compressed bitmap*/
    else {
#if LV_USE_FONT_COMPRESSED
#if LVGL_VERSION_MAJOR < 9
        static uint8_t *bitmap_out = NULL;
        static size_t last_buf_size = 0;
        uint32_t gsize = gdsc->box_w * gdsc->box_h;
        if (gsize == 0) {
            return NULL;
        }

        uint32_t buf_size = gsize;
        /*Compute memory size needed to hold decompressed glyph, rounding up*/
        switch (fdsc->bpp) {
        case 1:
            buf_size = (gsize + 7) >> 3;
            break;
        case 2:
            buf_size = (gsize + 3) >> 2;
            break;
        case 3:
            buf_size = (gsize + 1) >> 1;
            break;
        case 4:
            buf_size = (gsize + 1) >> 1;
            break;
        }

        if (last_buf_size < buf_size) {
            uint8_t * tmp = lv_mem_realloc(bitmap_out, buf_size);
            LV_ASSERT_MALLOC(tmp);
            if (tmp == NULL) {
                return NULL;
            }
            bitmap_out = tmp;
            last_buf_size = buf_size;
        }
#endif
        bool prefilter = fdsc->bitmap_format == D2_FONT_FMT_TXT_COMPRESSED;
        decompress(bitmap_in, bitmap_out, gdsc->box_w, gdsc->box_h,
                   (uint8_t)fdsc->bpp, prefilter);
#if LVGL_VERSION_MAJOR >= 9
        lv_draw_buf_flush_cache(draw_buf, NULL);
        return draw_buf;
#else
        return bitmap_out;
#endif
#else /*!LV_USE_FONT_COMPRESSED*/
        // LV_LOG_WARN("Compressed fonts is used but LV_USE_FONT_COMPRESSED is not enabled in lv_conf.h");
        return NULL;
#endif
    }

    /*If not returned earlier then the letter is not found in this font*/
    return NULL;
}

bool d2_font_get_glyph_dsc_fmt_txt(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode_letter,
                                   uint32_t unicode_letter_next)
{
    /*It fixes a strange compiler optimization issue: https://github.com/lvgl/lvgl/issues/4370*/
    bool is_tab = unicode_letter == '\t';
    if (is_tab) {
        unicode_letter = ' ';
    }
    d2_font_context_t *ctx = (d2_font_context_t *)font->user_data;
    d2_font_fmt_txt_dsc_t * fdsc = (d2_font_fmt_txt_dsc_t *)(ctx->base_ptr + (uint32_t)font->dsc);

    uint32_t gid = get_glyph_dsc_id(font, unicode_letter, NULL);
    if (!gid) {
        return false;
    }

    int8_t kvalue = 0;
    if (fdsc->kern_dsc) {
        uint32_t gid_next = get_glyph_dsc_id(font, unicode_letter_next, NULL);
        if (gid_next) {
            kvalue = get_kern_value(font, gid, gid_next);
        }
    }

    /*Put together a glyph dsc*/
    const d2_font_fmt_txt_glyph_index_t *gindex = (d2_font_fmt_txt_glyph_index_t *)(ctx->base_ptr + (uint32_t)fdsc->glyph_index) + gid;
    const d2_font_fmt_txt_glyph_dsc_t *gdsc = (d2_font_fmt_txt_glyph_dsc_t *)(ctx->base_ptr + (uint32_t)fdsc->glyph_dsc) + gindex->dsc_index;

    int32_t kv = ((int32_t)((int32_t)kvalue * fdsc->kern_scale) >> 4);

    uint32_t adv_w = gdsc->adv_w;
    if (is_tab) {
        adv_w *= 2;
    }

    adv_w += kv;
    adv_w  = (adv_w + (1 << 3)) >> 4;

    dsc_out->adv_w = adv_w;
    dsc_out->box_h = gdsc->box_h;
    dsc_out->box_w = gdsc->box_w;
    dsc_out->ofs_x = gdsc->ofs_x;
    dsc_out->ofs_y = gdsc->ofs_y;
#if LVGL_VERSION_MAJOR >= 9
    dsc_out->format = (uint8_t)fdsc->bpp;
    dsc_out->gid.index = unicode_letter;
#else
    dsc_out->bpp   = (uint8_t)fdsc->bpp;
#endif
    dsc_out->is_placeholder = false;

    if (is_tab) {
        dsc_out->box_w = dsc_out->box_w * 2;
    }

    return true;
}

static uint32_t get_glyph_dsc_id(const lv_font_t * font, uint32_t letter, const d2_font_fmt_txt_cmap_t **cmap)
{
    if (letter == '\0') {
        return 0;
    }

    d2_font_context_t *ctx = (d2_font_context_t *)font->user_data;

    for (size_t i = 0; i < sizeof(ctx->cache) / sizeof(ctx->cache[0]); i++) {
        if (letter == ctx->cache[i].unicode_letter) {
            d2_font_fmt_txt_glyph_cache_t temp_cache;
            if (i != 0) {
                temp_cache = ctx->cache[0];
                ctx->cache[0] = ctx->cache[1];
                ctx->cache[1] = temp_cache;
            }
            if (cmap) {
                *cmap = ctx->cache[0].cmap;
            }
            return ctx->cache[0].glyph_id;
        }
    }

    d2_font_fmt_txt_dsc_t * fdsc = (d2_font_fmt_txt_dsc_t *)(ctx->base_ptr + (uint32_t)font->dsc);
    const d2_font_fmt_txt_cmap_t * cmaps = (const d2_font_fmt_txt_cmap_t *)(ctx->base_ptr + (uint32_t)fdsc->cmaps);

    for (size_t i = 0; i < fdsc->cmap_num; i++) {
        /*Relative code point*/
        uint32_t rcp = letter - cmaps[i].range_start;
        if (rcp >= cmaps[i].range_length) {
            continue;
        }
        uint32_t glyph_id = 0;
        if (cmaps[i].type == D2_FONT_FMT_TXT_CMAP_FORMAT0_TINY) {
            glyph_id = cmaps[i].glyph_id_start + rcp;
        } else if (cmaps[i].type == D2_FONT_FMT_TXT_CMAP_FORMAT0_FULL) {
            const uint8_t * gid_ofs_8 = (const uint8_t *)(ctx->base_ptr + (uint32_t)cmaps[i].glyph_id_ofs_list);
            /* The first character is always valid and should have offset = 0
             * However if a character is missing it also has offset=0.
             * So if there is a 0 not on the first position then it's a missing character */
            if (gid_ofs_8[rcp] == 0 && letter != cmaps[i].range_start) {
                continue;
            }
            glyph_id = cmaps[i].glyph_id_start + gid_ofs_8[rcp];
        } else if (cmaps[i].type == D2_FONT_FMT_TXT_CMAP_SPARSE_TINY) {
            uint16_t key = rcp;
            const uint16_t *unicode_list = (const uint16_t *)(ctx->base_ptr + (uint32_t)cmaps[i].unicode_list);
            uint16_t * p = lv_utils_bsearch(&key, unicode_list, cmaps[i].list_length,
                                            sizeof(unicode_list[0]), unicode_list_compare);

            if (p) {
                lv_uintptr_t ofs = p - unicode_list;
                glyph_id = cmaps[i].glyph_id_start + (uint32_t) ofs;
            }
        } else if (cmaps[i].type == D2_FONT_FMT_TXT_CMAP_SPARSE_FULL) {
            uint16_t key = rcp;
            const uint16_t *unicode_list = (const uint16_t *)(ctx->base_ptr + (uint32_t)cmaps[i].unicode_list);
            uint16_t * p = lv_utils_bsearch(&key, unicode_list, cmaps[i].list_length,
                                            sizeof(unicode_list[0]), unicode_list_compare);

            if (p) {
                lv_uintptr_t ofs = p - unicode_list;
                const uint16_t * gid_ofs_16 = (const uint16_t *)(ctx->base_ptr + (uint32_t)cmaps[i].glyph_id_ofs_list);
                glyph_id = cmaps[i].glyph_id_start + gid_ofs_16[ofs];
            }
        }
        ctx->cache[1] = ctx->cache[0];
        ctx->cache[0].unicode_letter = letter;
        ctx->cache[0].glyph_id = glyph_id;
        ctx->cache[0].cmap = &cmaps[i];
        if (cmap) {
            *cmap = &cmaps[i];
        }
        return glyph_id;
    }

    return 0;

}

static int8_t get_kern_value(const lv_font_t * font, uint32_t gid_left, uint32_t gid_right)
{
    d2_font_context_t *ctx = (d2_font_context_t *)font->user_data;
    d2_font_fmt_txt_dsc_t * fdsc = (d2_font_fmt_txt_dsc_t *)(ctx->base_ptr + (uint32_t)font->dsc);

    int8_t value = 0;

    if (fdsc->kern_classes == 0) {
        /*Kern pairs*/
        const d2_font_fmt_txt_kern_pair_t *kdsc = (d2_font_fmt_txt_kern_pair_t *)(ctx->base_ptr + (uint32_t)fdsc->kern_dsc);
        if (gid_left > kdsc->glyph_id_max || gid_right > kdsc->glyph_id_max) {
            return 0;
        }
        /*Use binary search to find the kern value.
        *The pairs are ordered left_id first, then right_id secondly.*/
        const void *g_ids = ((void*)kdsc + sizeof(d2_font_fmt_txt_kern_pair_t) + kdsc->pair_cnt);
        if (kdsc->glyph_ids_size == 0) {
            kern_pair_ref_t g_id_both = {gid_left, gid_right};
            uint16_t * kid_p = lv_utils_bsearch(&g_id_both, g_ids, kdsc->pair_cnt, 2, kern_pair_8_compare);

            /*If the `g_id_both` were found get its index from the pointer*/
            if (kid_p) {
                lv_uintptr_t ofs = kid_p - (uint16_t *)g_ids;
                value = kdsc->values[ofs];
            }
        } else if (kdsc->glyph_ids_size == 1) {
            kern_pair_ref_t g_id_both = {gid_left, gid_right};
            uint32_t * kid_p = lv_utils_bsearch(&g_id_both, g_ids, kdsc->pair_cnt, 4, kern_pair_16_compare);

            /*If the `g_id_both` were found get its index from the pointer*/
            if (kid_p) {
                lv_uintptr_t ofs = kid_p - (uint32_t *)g_ids;
                value = kdsc->values[ofs];
            }

        } else {
            /*Invalid value*/
        }
    }
    return value;
}

#if LVGL_VERSION_MAJOR >= 9
/** Code Comparator.
 *
 *  Compares the value of both input arguments.
 *
 *  @param[in]  pRef        Pointer to the reference.
 *  @param[in]  pElement    Pointer to the element to compare.
 *
 *  @return Result of comparison.
 *  @retval < 0   Reference is less than element.
 *  @retval = 0   Reference is equal to element.
 *  @retval > 0   Reference is greater than element.
 *
 */
static int unicode_list_compare(const void * ref, const void * element)
{
    return (*(uint16_t *)ref) - (*(uint16_t *)element);
}

static int kern_pair_8_compare(const void * ref, const void * element)
{
    const kern_pair_ref_t * ref8_p = ref;
    const uint8_t * element8_p = element;

    /*If the MSB is different it will matter. If not return the diff. of the LSB*/
    if (ref8_p->gid_left != element8_p[0]) {
        return ref8_p->gid_left - element8_p[0];
    } else {
        return ref8_p->gid_right - element8_p[1];
    }
}

static int kern_pair_16_compare(const void * ref, const void * element)
{
    const kern_pair_ref_t * ref16_p = ref;
    const uint16_t * element16_p = element;

    /*If the MSB is different it will matter. If not return the diff. of the LSB*/
    if (ref16_p->gid_left != element16_p[0]) {
        return ref16_p->gid_left - element16_p[0];
    } else {
        return ref16_p->gid_right - element16_p[1];
    }
}

#else
static int32_t unicode_list_compare(const void * ref, const void * element)
{
    return ((int32_t)(*(uint16_t *)ref)) - ((int32_t)(*(uint16_t *)element));
}

static int32_t kern_pair_8_compare(const void * ref, const void * element)
{
    const uint8_t * ref8_p = ref;
    const uint8_t * element8_p = element;

    /*If the MSB is different it will matter. If not return the diff. of the LSB*/
    if (ref8_p[0] != element8_p[0]) {
        return (int32_t)ref8_p[0] - element8_p[0];
    } else {
        return (int32_t) ref8_p[1] - element8_p[1];
    }
}

static int32_t kern_pair_16_compare(const void * ref, const void * element)
{
    const uint16_t * ref16_p = ref;
    const uint16_t * element16_p = element;

    /*If the MSB is different it will matter. If not return the diff. of the LSB*/
    if (ref16_p[0] != element16_p[0]) {
        return (int32_t)ref16_p[0] - element16_p[0];
    } else {
        return (int32_t) ref16_p[1] - element16_p[1];
    }
}
#endif

#if LV_USE_FONT_COMPRESSED

/**
 * The compress a glyph's bitmap
 * @param in the compressed bitmap
 * @param out buffer to store the result
 * @param px_num number of pixels in the glyph (width * height)
 * @param bpp bit per pixel (bpp = 3 will be converted to bpp = 4)
 * @param prefilter true: the lines are XORed
 */
static void decompress(const uint8_t * in, uint8_t * out, int32_t w, int32_t h, uint8_t bpp, bool prefilter)
{
    d2_font_fmt_rle_t rle;

#if LVGL_VERSION_MAJOR >= 9
    const lv_opa_t * opa_table;
    switch (bpp) {
    case 2:
        opa_table = opa2_table;
        break;
    case 3:
        opa_table = opa3_table;
        break;
    case 4:
        opa_table = opa4_table;
        break;
    default:
        // LV_LOG_WARN("%d bpp is not handled", bpp);
        return;
    }
#else
    uint32_t wrp = 0;
    uint8_t wr_size = bpp;
    if (bpp == 3) {
        wr_size = 4;
    }
#endif

    rle_init(&rle, in, bpp);

    uint8_t * line_buf1 = malloc(w);

    uint8_t * line_buf2 = NULL;

    if (prefilter) {
        line_buf2 = malloc(w);
    }

    decompress_line(&rle, line_buf1, w);

#if LVGL_VERSION_MAJOR >= 9
    int32_t y;
    int32_t x;
    uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_A8);

    for (x = 0; x < w; x++) {
        out[x] = opa_table[line_buf1[x]];
    }
    out += stride;

    for (y = 1; y < h; y++) {
        if (prefilter) {
            decompress_line(&rle, line_buf2, w);

            for (x = 0; x < w; x++) {
                line_buf1[x] = line_buf2[x] ^ line_buf1[x];
                out[x] = opa_table[line_buf1[x]];
            }
        } else {
            decompress_line(&rle, line_buf1, w);

            for (x = 0; x < w; x++) {
                out[x] = opa_table[line_buf1[x]];
            }
        }
        out += stride;
    }
#else
    lv_coord_t y;
    lv_coord_t x;

    for (x = 0; x < w; x++) {
        bits_write(out, wrp, line_buf1[x], bpp);
        wrp += wr_size;
    }

    for (y = 1; y < h; y++) {
        if (prefilter) {
            decompress_line(&rle, line_buf2, w);

            for (x = 0; x < w; x++) {
                line_buf1[x] = line_buf2[x] ^ line_buf1[x];
                bits_write(out, wrp, line_buf1[x], bpp);
                wrp += wr_size;
            }
        } else {
            decompress_line(&rle, line_buf1, w);

            for (x = 0; x < w; x++) {
                bits_write(out, wrp, line_buf1[x], bpp);
                wrp += wr_size;
            }
        }
    }

#endif

    free(line_buf1);
    free(line_buf2);
}

/**
 * Decompress one line. Store one pixel per byte
 * @param out output buffer
 * @param w width of the line in pixel count
 */
static inline void decompress_line(d2_font_fmt_rle_t *rle, uint8_t * out, int32_t w)
{
    int32_t i;
    for (i = 0; i < w; i++) {
        out[i] = rle_next(rle);
    }
}

static inline void rle_init(d2_font_fmt_rle_t *rle, const uint8_t * in,  uint8_t bpp)
{
    rle->in = in;
    rle->bpp = bpp;
    rle->state = D2_RLE_STATE_SINGLE;
    rle->rdp = 0;
    rle->prev_v = 0;
    rle->count = 0;
}

static inline uint8_t rle_next(d2_font_fmt_rle_t *rle)
{
    uint8_t v = 0;
    uint8_t ret = 0;

    if (rle->state == D2_RLE_STATE_SINGLE) {
        ret = get_bits(rle->in, rle->rdp, rle->bpp);
        if (rle->rdp != 0 && rle->prev_v == ret) {
            rle->count = 0;
            rle->state = D2_RLE_STATE_REPEATED;
        }

        rle->prev_v = ret;
        rle->rdp += rle->bpp;
    } else if (rle->state == D2_RLE_STATE_REPEATED) {
        v = get_bits(rle->in, rle->rdp, 1);
        rle->count++;
        rle->rdp += 1;
        if (v == 1) {
            ret = rle->prev_v;
            if (rle->count == 11) {
                rle->count = get_bits(rle->in, rle->rdp, 6);
                rle->rdp += 6;
                if (rle->count != 0) {
                    rle->state = D2_RLE_STATE_COUNTER;
                } else {
                    ret = get_bits(rle->in, rle->rdp, rle->bpp);
                    rle->prev_v = ret;
                    rle->rdp += rle->bpp;
                    rle->state = D2_RLE_STATE_SINGLE;
                }
            }
        } else {
            ret = get_bits(rle->in, rle->rdp, rle->bpp);
            rle->prev_v = ret;
            rle->rdp += rle->bpp;
            rle->state = D2_RLE_STATE_SINGLE;
        }

    } else if (rle->state == D2_RLE_STATE_COUNTER) {
        ret = rle->prev_v;
        rle->count--;
        if (rle->count == 0) {
            ret = get_bits(rle->in, rle->rdp, rle->bpp);
            rle->prev_v = ret;
            rle->rdp += rle->bpp;
            rle->state = D2_RLE_STATE_SINGLE;
        }
    }

    return ret;
}

/**
 * Read bits from an input buffer. The read can cross byte boundary.
 * @param in the input buffer to read from.
 * @param bit_pos index of the first bit to read.
 * @param len number of bits to read (must be <= 8).
 * @return the read bits
 */
static inline uint8_t get_bits(const uint8_t * in, uint32_t bit_pos, uint8_t len)
{
    uint8_t bit_mask;
    switch (len) {
    case 1:
        bit_mask = 0x1;
        break;
    case 2:
        bit_mask = 0x3;
        break;
    case 3:
        bit_mask = 0x7;
        break;
    case 4:
        bit_mask = 0xF;
        break;
    case 8:
        bit_mask = 0xFF;
        break;
    default:
        bit_mask = (uint16_t)((uint16_t) 1 << len) - 1;
    }

    uint32_t byte_pos = bit_pos >> 3;
    bit_pos = bit_pos & 0x7;

    if (bit_pos + len >= 8) {
        uint16_t in16 = (in[byte_pos] << 8) + in[byte_pos + 1];
        return (in16 >> (16 - bit_pos - len)) & bit_mask;
    } else {
        return (in[byte_pos] >> (8 - bit_pos - len)) & bit_mask;
    }
}

#if LVGL_VERSION_MAJOR < 9
/**
* Write `val` data to `bit_pos` position of `out`. The write can NOT cross byte boundary.
* @param out buffer where to write
* @param bit_pos bit index to write
* @param val value to write
* @param len length of bits to write from `val`. (Counted from the LSB).
* @note `len == 3` will be converted to `len = 4` and `val` will be upscaled too
*/
static inline void bits_write(uint8_t * out, uint32_t bit_pos, uint8_t val, uint8_t len)
{
    if (len == 3) {
        len = 4;
        switch (val) {
        case 0:
            val = 0;
            break;
        case 1:
            val = 2;
            break;
        case 2:
            val = 4;
            break;
        case 3:
            val = 6;
            break;
        case 4:
            val = 9;
            break;
        case 5:
            val = 11;
            break;
        case 6:
            val = 13;
            break;
        case 7:
            val = 15;
            break;
        }
    }

    uint16_t byte_pos = bit_pos >> 3;
    bit_pos = bit_pos & 0x7;
    bit_pos = 8 - bit_pos - len;

    uint8_t bit_mask = (uint16_t)((uint16_t) 1 << len) - 1;
    out[byte_pos] &= ((~bit_mask) << bit_pos);
    out[byte_pos] |= (val << bit_pos);
}
#endif
#endif /*LV_USE_FONT_COMPRESSED*/
