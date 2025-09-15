#include "hex_utils.h"
#include <ctype.h>
#include <string.h>

/* 十六进制字符转换为数值 */
uint8_t hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* 检查字符串是否为有效的十六进制 */
bool hex_is_valid(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isxdigit((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

/* 十六进制字符串转换为普通字符串 */
int hex_to_ascii(const char *hex, char *output, size_t output_size) {
    int hex_len = strlen(hex);
    if (hex_len % 2 != 0 || output_size < hex_len / 2 + 1) {
        return -1;
    }
    
    int j = 0;
    for (int i = 0; i < hex_len; i += 2) {
        uint8_t high = hex_char_to_value(hex[i]);
        uint8_t low = hex_char_to_value(hex[i + 1]);
        output[j++] = (high << 4) | low;
    }
    output[j] = '\0';
    return j;
}