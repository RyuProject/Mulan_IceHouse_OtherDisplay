/**
 * @file utf8_validator.h
 * @brief UTF-8编码验证工具
 */

#ifndef UTF8_VALIDATOR_H
#define UTF8_VALIDATOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief 验证字符串是否为有效的UTF-8编码
 * 
 * @param data 要验证的数据
 * @param length 数据长度
 * @return true 如果是有效的UTF-8编码
 * @return false 如果不是有效的UTF-8编码
 */
bool utf8_is_valid(const uint8_t *data, size_t length);

#endif /* UTF8_VALIDATOR_H */