#ifndef HEX_UTILS_H
#define HEX_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 十六进制字符转换为数值
uint8_t hex_char_to_value(char c);

// 检查字符串是否为有效的十六进制
bool hex_is_valid(const char *str);

// 十六进制字符串转换为普通字符串
int hex_to_ascii(const char *hex, char *output, size_t output_size);

#endif // HEX_UTILS_H