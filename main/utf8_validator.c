/**
 * @file utf8_validator.c
 * @brief UTF-8编码验证工具实现
 */

#include "utf8_validator.h"
#include "esp_log.h"

static const char *TAG = "UTF8_VALIDATOR";

/**
 * @brief 验证字符串是否为有效的UTF-8编码
 * 
 * UTF-8编码规则：
 * - 单字节字符: 0xxxxxxx (ASCII范围: 0x00-0x7F)
 * - 双字节字符: 110xxxxx 10xxxxxx (范围: 0xC0-0xDF 0x80-0xBF)
 * - 三字节字符: 1110xxxx 10xxxxxx 10xxxxxx (范围: 0xE0-0xEF 0x80-0xBF 0x80-0xBF)
 * - 四字节字符: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (范围: 0xF0-0xF7 0x80-0xBF 0x80-0xBF 0x80-0xBF)
 * 
 * @param data 要验证的数据
 * @param length 数据长度
 * @return true 如果是有效的UTF-8编码
 * @return false 如果不是有效的UTF-8编码
 */
bool utf8_is_valid(const uint8_t *data, size_t length) {
    bool is_valid_utf8 = true;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t c = data[i];
        if (c > 0x7F) { // 非ASCII字符
            // 检查UTF-8编码有效性
            if ((c & 0xE0) == 0xC0) { // 2字节UTF-8
                if (i + 1 >= length || (data[i+1] & 0xC0) != 0x80) {
                    is_valid_utf8 = false;
                    break;
                }
                i++; // 跳过下一个字节
            } else if ((c & 0xF0) == 0xE0) { // 3字节UTF-8
                if (i + 2 >= length || 
                    (data[i+1] & 0xC0) != 0x80 ||
                    (data[i+2] & 0xC0) != 0x80) {
                    is_valid_utf8 = false;
                    break;
                }
                i += 2; // 跳过两个字节
            } else if ((c & 0xF8) == 0xF0) { // 4字节UTF-8
                if (i + 3 >= length || 
                    (data[i+1] & 0xC0) != 0x80 ||
                    (data[i+2] & 0xC0) != 0x80 ||
                    (data[i+3] & 0xC0) != 0x80) {
                    is_valid_utf8 = false;
                    break;
                }
                i += 3; // 跳过三个字节
            } else {
                is_valid_utf8 = false;
                break;
            }
        }
    }
    
    return is_valid_utf8;
}