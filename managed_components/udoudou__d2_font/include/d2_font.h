/*
 * SPDX-FileCopyrightText: 2025 udoudou
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "src/font/lv_font.h"

/**
 * Loads a `lv_font_t` object from partition.
 * @param label Partition label where d2_font bin is stored.
 * @return a pointer to the font or NULL in case of error.
 */
lv_font_t * d2_font_load_from_partition(const char* label);

/**
 * Loads a `lv_font_t` object from partition.
 *
 * Note: The memory address space passed in should remain accessible until the `lv_font_t` object is freed.
 *
 * @param bin_ptr a pointer to d2_font bin. It can be the address obtained by mmap.
 * @param size d2_font bin size.
 * @return a pointer to the font or NULL in case of error.
 */
lv_font_t *d2_font_load_from_mem(const uint8_t *bin_ptr, size_t size);

/**
 * Unload a `lv_font_t` object from `d2_font_load_xx`.
 * @param font `lv_font_t` object.
 */
void d2_font_unload(lv_font_t * font);

#ifdef __cplusplus
} /*extern "C"*/
#endif
