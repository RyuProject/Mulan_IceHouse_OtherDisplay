/*
 * SPDX-FileCopyrightText: 2025 udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "d2_font.h"

#include "string.h"
#include "mbedtls/sha256.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "d2_font_fmt_txt.h"

static const char *TAG = "d2_font";
typedef struct {
    uint32_t version;
    int32_t line_height;
    int32_t base_line;
    uint8_t subpx;
    int8_t underline_position;
    int8_t underline_thickness;
    uint8_t padding;
} __attribute__((packed)) d2_font_header_bin_t;

lv_font_t *d2_font_load_from_mem(const uint8_t *bin_ptr, size_t size)
{
    const void *data;
    if (bin_ptr == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid param");
        return NULL;
    }

    /*header*/
    data = bin_ptr;
    uint16_t header_length = *(uint16_t *)data;
    data += sizeof(uint16_t);
    if (memcmp(data, "D2FtHd", 6) != 0 || header_length < 8 + sizeof(d2_font_header_bin_t)) {
        ESP_LOGE(TAG, "Header error");
        return NULL;
    }
    data += 6;
    const d2_font_header_bin_t *font_header = (const d2_font_header_bin_t *)data;

    /*dsc*/
    if (header_length + 4 + sizeof(d2_font_fmt_txt_dsc_t) > size) {
        ESP_LOGE(TAG, "Header_length error");
        return NULL;
    }
    data = bin_ptr + header_length;
    uint32_t dsc_length = *(uint32_t *)data;
    data += sizeof(uint32_t);
    const d2_font_fmt_txt_dsc_t *fdsc = (const d2_font_fmt_txt_dsc_t *)data;

    /*sha256*/
    if (header_length + dsc_length + 32 > size) {
        ESP_LOGE(TAG, "Dsc_length error");
        return NULL;
    }
    data = bin_ptr + header_length + dsc_length;
    uint8_t *sha256 = (uint8_t *)data;

    uint8_t sha256_calc[32] = { 0 };
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, false);
    mbedtls_sha256_update(&sha256_ctx, bin_ptr, header_length + dsc_length);
    mbedtls_sha256_finish(&sha256_ctx, sha256_calc);
    mbedtls_sha256_free(&sha256_ctx);

    if (memcmp(sha256, sha256_calc, 32) != 0) {
        ESP_LOGE(TAG, "SHA256 error");
        return NULL;
    }

    /* Check each table address */
    const d2_font_fmt_txt_cmap_t *cmaps = (const d2_font_fmt_txt_cmap_t *)((uint8_t *)fdsc + (uint32_t)fdsc->cmaps);
    if ((uint8_t *)cmaps - bin_ptr > size || memcpy((uint8_t *)cmaps - 4, "CMAP", 4) == 0) {
        ESP_LOGE(TAG, "cmaps error");
        return NULL;
    }

    const d2_font_fmt_txt_kern_pair_t *kdsc = (d2_font_fmt_txt_kern_pair_t *)((uint8_t *)fdsc + (uint32_t)fdsc->kern_dsc);
    if ((uint8_t *)kdsc - bin_ptr > size || memcpy((uint8_t *)kdsc - 4, "KERN", 4) == 0) {
        ESP_LOGE(TAG, "kdsc error");
        return NULL;
    }

    const d2_font_fmt_txt_glyph_index_t *gindex = (d2_font_fmt_txt_glyph_index_t *)((uint8_t *)fdsc + (uint32_t)fdsc->glyph_index);
    if ((uint8_t *)gindex - bin_ptr > size || memcpy((uint8_t *)gindex - 4, "GIDX", 4) == 0) {
        ESP_LOGE(TAG, "gindex error");
        return NULL;
    }

    const d2_font_fmt_txt_glyph_dsc_t *gdsc = (d2_font_fmt_txt_glyph_dsc_t *)((uint8_t *)fdsc + (uint32_t)fdsc->glyph_dsc);
    if ((uint8_t *)gdsc - bin_ptr > size || memcpy((uint8_t *)gdsc - 4, "GDSC", 4) == 0) {
        ESP_LOGE(TAG, "gindex error");
        return NULL;
    }

    const uint8_t *bitmap_in = (uint8_t *)((uint8_t *)fdsc + (uint32_t)fdsc->glyph_bitmap);
    if ((uint8_t *)bitmap_in - bin_ptr > size || memcpy((uint8_t *)bitmap_in - 4, "GBIT", 4) == 0) {
        ESP_LOGE(TAG, "gindex error");
        return NULL;
    }

    lv_font_t *font = (lv_font_t *)heap_caps_calloc(1, sizeof(lv_font_t) + sizeof(d2_font_context_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (font == NULL) {
        ESP_LOGE(TAG, "malloc failed");
        return NULL;
    }
    font->get_glyph_dsc = d2_font_get_glyph_dsc_fmt_txt;
    font->get_glyph_bitmap = d2_font_get_bitmap_fmt_txt;
    d2_font_context_t *ctx = (d2_font_context_t *)(font + 1);
    ctx->base_ptr = (uint8_t *)fdsc;

    font->user_data = (void *)ctx;

    font->line_height = font_header->line_height;
    font->base_line = font_header->base_line;
    font->subpx = font_header->subpx;
    font->underline_position = font_header->underline_position;
    font->underline_thickness = font_header->underline_thickness;

    return font;
}

lv_font_t *d2_font_load_from_partition(const char *label)
{
    esp_err_t err;
    if (label == NULL) {
        return NULL;
    }
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition not found");
        return NULL;
    }
    const void *map_ptr;
    esp_partition_mmap_handle_t map_handle;
    err = esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &map_ptr, &map_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Partition mmap failed");
        return NULL;
    }
    lv_font_t *font = d2_font_load_from_mem(map_ptr, partition->size);
    if (font == NULL) {
        goto error;
    }
    d2_font_context_t *ctx = font->user_data;
    ctx->mmap_handle = (void *)map_handle;
    return font;
error:
    esp_partition_munmap(map_handle);
    return NULL;
}

void d2_font_unload(lv_font_t *font)
{
    d2_font_context_t *ctx = (d2_font_context_t *)font->user_data;
    esp_partition_mmap_handle_t mmap_handle = (esp_partition_mmap_handle_t)ctx->mmap_handle;
    if (mmap_handle) {
        esp_partition_munmap(mmap_handle);
    }
    heap_caps_free(font);
}
