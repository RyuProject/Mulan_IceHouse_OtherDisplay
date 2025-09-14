
/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "lv_demos.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "cJSON.h"
#include "lv_font_mulan_14.c"
#include <ctype.h>
#include <stdlib.h>

static const char *TAG = "NimBLE_BLE_PRPH";

/* 十六进制字符转换为数值 */
static uint8_t hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

/* 检查字符串是否为有效的十六进制 */
static bool is_valid_hex(const char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isxdigit((unsigned char)str[i])) {
            return false;
        }
    }
    return true;
}

/* 十六进制字符串转换为普通字符串 */
static int hex_to_string(const char *hex, char *output, size_t output_size) {
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

// 定时器回调函数声明
static void popup_timer_cb(lv_timer_t *timer);
static void show_popup_message(const char *message, uint32_t duration_ms);

static lv_obj_t *order_card = NULL;
static lv_obj_t *orders_container = NULL;
LV_FONT_DECLARE(lv_font_mulan_14);

// 动态订单列表相关函数声明
static void create_dynamic_order_row(int order_num, const char *dishes);
static void btn_ready_cb(lv_event_t *e);

// 定时器回调函数实现
static void popup_timer_cb(lv_timer_t *timer) {
    lv_obj_t *popup = (lv_obj_t *)lv_timer_get_user_data(timer);
    lv_obj_del(popup);
    lv_timer_del(timer);
}

static void show_popup_message(const char *message, uint32_t duration_ms)
{
    bsp_display_lock(0);
    
    // 创建弹出窗口
    lv_obj_t *popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(popup, 300, 100);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_80, 0);
    lv_obj_set_style_border_width(popup, 2, 0);
    lv_obj_set_style_border_color(popup, lv_color_white(), 0);
    lv_obj_set_style_radius(popup, 10, 0);
    
    // 创建消息标签
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_font(label, &lv_font_mulan_14, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    
    bsp_display_unlock();
    
    // 设置定时器自动关闭弹出窗口
    lv_timer_t *timer = lv_timer_create(popup_timer_cb, duration_ms, popup);
    lv_timer_set_repeat_count(timer, 1);
}

// 按钮点击事件：已出餐 → 修改按钮状态、文字、颜色
static void btn_ready_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    // 修改按钮文字为 "✅ 已出餐"
    lv_obj_t *label = lv_obj_get_child(btn, 0);  // 获取按钮内的 label
    if (label) {
        lv_label_set_text(label, "✅ 已出餐");
    }

    // 修改按钮背景色为灰色
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xCCCCCC), 0);

    // 禁用按钮交互（变灰且不可点击）
    lv_obj_clear_state(btn, LV_STATE_DEFAULT);          // 清除默认可点击状态
    lv_obj_add_state(btn, LV_STATE_DISABLED);           // 或直接禁用
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);      // 确保不可点击

    ESP_LOGI(TAG, "✅ 已出餐按钮被点击并已禁用");
}

// 创建订单行（动态添加到 UI）
static void create_dynamic_order_row(int order_num, const char *dishes) {
    // lv_obj_t *row = lv_obj_create(orders_container);
    // lv_obj_set_size(row, LV_PCT(100), 80);
    // lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    // lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);
    // lv_obj_set_style_pad_all(row, 10, 0);
    // lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // 左侧：订单信息（垂直）
    lv_obj_t *left_container = lv_obj_create(orders_container);
    lv_obj_set_flex_flow(left_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_width(left_container, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(left_container, 5, 0);

    lv_obj_t *order_label = lv_label_create(left_container);
    lv_label_set_text_fmt(order_label, "第 %d 单", order_num);
    lv_obj_set_style_text_font(order_label, &lv_font_mulan_14, 0);
    lv_obj_align(order_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *dish_label = lv_label_create(left_container);
    lv_label_set_text_fmt(dish_label, "菜品：%s", dishes);
    lv_obj_set_style_text_font(dish_label, &lv_font_mulan_14, 0);
    lv_obj_align(dish_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // 右侧：已出餐按钮
    // lv_obj_t *btn_ready = lv_btn_create(row);
    // lv_obj_set_size(btn_ready, 80, 30);
    // lv_obj_align(btn_ready, LV_ALIGN_TOP_RIGHT, -5, 5);
    // lv_obj_set_style_bg_color(btn_ready, lv_color_hex(0x007AFF), 0);  // 初始为蓝色
    // lv_obj_set_style_radius(btn_ready, 4, 0);
    // lv_obj_clear_flag(btn_ready, LV_OBJ_FLAG_SCROLLABLE);

    // // 按钮文字
    // lv_obj_t *btn_label = lv_label_create(btn_ready);
    // lv_label_set_text(btn_label, "已出餐");
    // lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    // lv_obj_center(btn_label);

    // // 添加点击事件
    // lv_obj_add_event_cb(btn_ready, btn_ready_cb, LV_EVENT_CLICKED, NULL);
}

// 初始化订单列表 UI
static void create_order_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xf5f5f5), 0);

    // 创建订单容器
    orders_container = lv_obj_create(scr);
    lv_obj_set_size(orders_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(orders_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(orders_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(orders_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(orders_container, 10, 0);

    // 初始显示等待数据
    lv_obj_t *waiting_label = lv_label_create(orders_container);
    lv_obj_set_style_text_font(waiting_label, &lv_font_mulan_14, 0);
    lv_label_set_text(waiting_label, "等待订单数据...");
    lv_obj_center(waiting_label);
}

/* 蓝牙相关变量和函数声明 */
static ble_uuid16_t gatt_svc_uuid = BLE_UUID16_INIT(0xABCD);
static ble_uuid16_t gatt_chr_uuid = BLE_UUID16_INIT(0x1234);

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static void bleprph_advertise(void);
static void bleprph_on_sync(void);
static void bleprph_on_reset(int reason);
static void bleprph_host_task(void *param);
static int bleprph_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);

/* 自定义 GATT 服务定义 */
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t *)&gatt_svc_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = (ble_uuid_t *)&gatt_chr_uuid,
                .access_cb = bleprph_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                .val_handle = 0,
            },
            {0}
        },
    },
    {0}
};

/* 蓝牙特征访问回调 */
static int bleprph_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint8_t buf[1024];  // 增加缓冲区大小
        uint16_t out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_hs_mbuf_to_flat failed: %d", rc);
            return BLE_ATT_ERR_UNLIKELY;
        }
        
        if (out_len < sizeof(buf)) {
            buf[out_len] = '\0';
        } else {
            buf[sizeof(buf) - 1] = '\0';
            out_len = sizeof(buf) - 1;
        }
        
        ESP_LOGI(TAG, "Received data length: %d bytes", out_len);
        
        // 检查是否数据完整（应该以}结尾）
        if (out_len > 0 && buf[out_len - 1] != '}') {
            ESP_LOGW(TAG, "Data appears to be truncated! Last byte: 0x%02X", buf[out_len - 1]);
            ESP_LOGW(TAG, "This might be a fragmented BLE packet. Ensure complete JSON is sent in one packet.");
        }
        
        // 检查数据是否为有效的UTF-8编码
        bool is_valid_utf8 = true;
        for (int i = 0; i < out_len; i++) {
            uint8_t c = buf[i];
            if (c > 0x7F) { // 非ASCII字符
                // 检查UTF-8编码有效性
                if ((c & 0xE0) == 0xC0) { // 2字节UTF-8
                    if (i + 1 >= out_len || (buf[i+1] & 0xC0) != 0x80) {
                        is_valid_utf8 = false;
                        break;
                    }
                    i++; // 跳过下一个字节
                } else if ((c & 0xF0) == 0xE0) { // 3字节UTF-8
                    if (i + 2 >= out_len || 
                        (buf[i+1] & 0xC0) != 0x80 ||
                        (buf[i+2] & 0xC0) != 0x80) {
                        is_valid_utf8 = false;
                        break;
                    }
                    i += 2; // 跳过两个字节
                } else if ((c & 0xF8) == 0xF0) { // 4字节UTF-8
                    if (i + 3 >= out_len || 
                        (buf[i+1] & 0xC0) != 0x80 ||
                        (buf[i+2] & 0xC0) != 0x80 ||
                        (buf[i+3] & 0xC0) != 0x80) {
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
        
        if (!is_valid_utf8) {
            ESP_LOGW(TAG, "Data encoding issue: Not valid UTF-8");
            ESP_LOGW(TAG, "This might be due to encoding mismatch between sender and receiver");
        }
        
        ESP_LOGI(TAG, "Received data: %.*s", out_len, (char *)buf);
        // 调试：打印原始字节数据
        ESP_LOGI(TAG, "Raw bytes (first 50 bytes):");        
        // 检查BLE MTU大小
        // uint16_t mtu = ble_att_mtu(conn_handle);
        
        // 如果数据长度异常，提供调试建议
        if (out_len < 20 || out_len > 256) {
            ESP_LOGW(TAG, "Unexpected data length. Check if data is properly formatted JSON");
            ESP_LOGW(TAG, "Expected format: {\"type\":\"info\",\"content\":\"小程序已连接\"}");
        }
        
        cJSON *root = cJSON_Parse((char *)buf);
        if (!root) {
            ESP_LOGW(TAG, "JSON parse failed");
            // 调试：打印解析失败的具体原因
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL) {
                ESP_LOGE(TAG, "JSON parse error before: %s", error_ptr);
            }
            
            // 检查是否为十六进制编码的数据（小程序发送的格式）
            if (strstr((char *)buf, "content") && strstr((char *)buf, "\"") && (strstr((char *)buf, "type") || strstr((char *)buf, "_id"))) {
                ESP_LOGI(TAG, "Detected possible hex-encoded content from mini program");
                
                // 尝试提取十六进制内容并解码
                char *content_start = strstr((char *)buf, "content");
                if (content_start) {
                    char *quote_start = strchr(content_start, '"');
                    if (quote_start) {
                        char *quote_end = strchr(quote_start + 1, '"');
                        if (quote_end) {
                            *quote_end = '\0';
                            char *hex_content = quote_start + 1;
                            
                            // 检查是否为有效的十六进制字符串
                            int hex_len = strlen(hex_content);
                            if (hex_len % 2 == 0 && is_valid_hex(hex_content)) {
                                ESP_LOGI(TAG, "Hex content detected: %s", hex_content);
                                
                                // 解码十六进制
                                char decoded_content[256] = {0};
                                int decoded_len = hex_to_string(hex_content, decoded_content, sizeof(decoded_content));
                                
                                if (decoded_len > 0) {
                                    ESP_LOGW(TAG, "Decoded content: %s", decoded_content);
                                    
                                    // 检查是否为系统消息
                                    if (strstr((char *)buf, "\"_id\":\"1\"") || strstr((char *)buf, "\"type\":\"info\"")) {
                                        ESP_LOGW(TAG, "收到系统消息: %s", decoded_content);
                                        show_popup_message(decoded_content, 3000);
                                    }
                                    *quote_end = '\"'; // 恢复原始字符串
                                    cJSON_Delete(root); // 释放JSON对象
                                    return 0;
                                } else {
                                    ESP_LOGE(TAG, "十六进制解码失败，hex_content: %s", hex_content);
                                }
                            }
                            *quote_end = '"'; // 恢复原始字符串
                        }
                    }
                }
            }
            
            return BLE_ATT_ERR_UNLIKELY;
        }

        bool is_message = false;
        bool is_order = false;
        
        // 检查是否为消息类型(type == "info")
        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (type && cJSON_IsString(type)) {
            if (strcmp(type->valuestring, "info") == 0) {
                is_message = true;
            } else if (strcmp(type->valuestring, "order") == 0) {
                is_order = true;
                // 兼容两种格式：有command字段和没有command字段的订单
                cJSON *command = cJSON_GetObjectItem(root, "command");
                if (command && cJSON_IsString(command)) {
                    // 如果有command字段，检查是否为display_order
                    if (strcmp(command->valuestring, "display_order") != 0) {
                        is_order = false; // 不是要显示的订单
                    }
                }
                // 如果没有command字段，默认认为是显示订单
            }
        }
        
        if (is_message) {
            // 处理消息类型
            cJSON *content = cJSON_GetObjectItem(root, "content");
            if (content && cJSON_IsString(content)) {
                char *content_str = content->valuestring;
                ESP_LOGI(TAG, "收到系统消息: %s", content_str);
                
                // 检查是否为十六进制编码的内容
                char decoded_content[256] = {0};
                int decoded_len = 0;
                int hex_len = strlen(content_str);
                if (hex_len % 2 == 0 && is_valid_hex(content_str)) {
                    ESP_LOGI(TAG, "检测到十六进制编码内容: %s", content_str);
                    
                    // 解码十六进制
                    decoded_len = hex_to_string(content_str, decoded_content, sizeof(decoded_content));
                    
                    if (decoded_len > 0) {
                        ESP_LOGI(TAG, "解码后的内容: %s", decoded_content);
                        
                        // 使用解码后的内容
                        content_str = decoded_content;
                    }
                }
                
                // 加锁保护UI更新
                bsp_display_lock(portMAX_DELAY);
                
                // 创建弹出式窗口
                lv_obj_t *popup = lv_obj_create(lv_scr_act());
                lv_obj_set_size(popup, 280, 120);
                lv_obj_align(popup, LV_ALIGN_CENTER, 0, 0);
                lv_obj_set_style_bg_color(popup, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_border_width(popup, 2, 0);
                lv_obj_set_style_border_color(popup, lv_color_hex(0x4CAF50), 0);
                lv_obj_set_style_radius(popup, 10, 0);
                lv_obj_set_style_pad_all(popup, 20, 0);
                
                // 创建消息文本
                lv_obj_t *msg_label = lv_label_create(popup);
                lv_obj_set_style_text_font(msg_label, &lv_font_mulan_14, 0);
                // 使用解码后的内容（如果有），否则使用原始内容
                if (decoded_len > 0) {
                    lv_label_set_text(msg_label, decoded_content);
                } else {
                    lv_label_set_text(msg_label, content_str);
                }
                lv_obj_set_style_text_color(msg_label, lv_color_hex(0x333333), 0);
                lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, 0);
                
                // 3秒后自动关闭窗口
                lv_timer_t *timer = lv_timer_create(popup_timer_cb, 3000, popup);
                lv_timer_set_repeat_count(timer, 1);
                
                bsp_display_unlock();
            }
        }

        if (is_order) {
            // 处理订单数据
            // 加锁保护UI更新
            bsp_display_lock(portMAX_DELAY);
            
            // 清空容器并重新创建内容
            lv_obj_clean(orders_container);
            
            // 解析订单ID
            cJSON *id = cJSON_GetObjectItem(root, "orderId");
            if (!id) {
                id = cJSON_GetObjectItem(root, "_id"); // 兼容旧格式
            }
            
            // 解析菜品内容并构建菜品字符串（优化内存使用）
            cJSON *items = cJSON_GetObjectItem(root, "items");
            char *dishes_str = NULL;
            size_t dishes_len = 0;
            size_t dishes_capacity = 256;
            
            if (items && cJSON_IsArray(items)) {
                dishes_str = malloc(dishes_capacity);
                if (!dishes_str) {
                    ESP_LOGE(TAG, "内存分配失败");
                    bsp_display_unlock();
                    cJSON_Delete(root);
                    return 0;
                }
                dishes_str[0] = '\0';
                
                cJSON *item = NULL;
                int item_count = 0;
                cJSON_ArrayForEach(item, items) {
                    cJSON *name = cJSON_GetObjectItem(item, "name");
                    if (cJSON_IsString(name)) {
                        char *name_str = name->valuestring;
                        char decoded_name[128] = {0}; // 减小临时缓冲区
                        int hex_len = strlen(name_str);
                        
                        if (hex_len % 2 == 0 && is_valid_hex(name_str)) {
                            ESP_LOGI(TAG, "检测到十六进制编码的菜品名: %s", name_str);
                            int decoded_len = hex_to_string(name_str, decoded_name, sizeof(decoded_name));
                            if (decoded_len > 0) {
                                ESP_LOGI(TAG, "解码后的菜品名: %s", decoded_name);
                                name_str = decoded_name;
                            }
                        }
                        
                        // 动态调整缓冲区大小
                        size_t needed_len = dishes_len + (item_count > 0 ? 3 : 0) + strlen(name_str) + 1;
                        if (needed_len > dishes_capacity) {
                            dishes_capacity = needed_len * 2;
                            char *new_dishes = realloc(dishes_str, dishes_capacity);
                            if (!new_dishes) {
                                ESP_LOGE(TAG, "内存重新分配失败");
                                free(dishes_str);
                                bsp_display_unlock();
                                cJSON_Delete(root);
                                return 0;
                            }
                            dishes_str = new_dishes;
                        }
                        
                        if (item_count > 0) {
                            strcat(dishes_str, "、");
                            dishes_len += 3; // 中文字符占3字节
                        }
                        strcat(dishes_str, name_str);
                        dishes_len += strlen(name_str);
                        item_count++;
                    }
                }
            }
            
            // 生成订单号（基于订单ID或时间戳）
            int order_num = 1;
            if (id && cJSON_IsString(id)) {
                // 使用订单ID的后几位作为订单号
                char *id_str = id->valuestring;
                int len = strlen(id_str);
                if (len > 4) {
                    order_num = atoi(id_str + len - 4);
                } else {
                    order_num = atoi(id_str);
                }
                if (order_num <= 0) order_num = 1;
            }
            
            // 创建动态订单行
            create_dynamic_order_row(order_num, dishes_str);
            
            // 释放动态分配的内存
            if (dishes_str) {
                free(dishes_str);
            }
            
            bsp_display_unlock();
        }

        cJSON_Delete(root);
        return 0;
    }
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        const char *resp = "OK";
        int rc = os_mbuf_append(ctxt->om, resp, strlen(resp));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* 蓝牙GAP事件处理 */
static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected, handle=%d", event->connect.conn_handle);
        } else {
            ESP_LOGI(TAG, "Connect failed; status=%d", event->connect.status);
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete");
        bleprph_advertise();
        return 0;

    default:
        return 0;
    }
}

/* 蓝牙广播 */
static void bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    const char *name = "MuLan";
    int rc;
    uint8_t own_addr_type;

    ble_hs_util_ensure_addr(0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer addr type failed; rc=%d", rc);
        return;
    }

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    fields.uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(0xABCD) };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rsp_fields.uuids16 = (ble_uuid16_t[]){ BLE_UUID16_INIT(0xABCD) };
    rsp_fields.num_uuids16 = 1;
    rsp_fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv set fields failed; rc=%d", rc);
        return;
    }
    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv rsp set fields failed; rc=%d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv start failed; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started: %s", name);
}

/* 蓝牙同步回调 */
static void bleprph_on_sync(void)
{
    uint8_t own_addr_type;
    uint8_t addr_val[6];
    int rc;

    ble_hs_util_ensure_addr(0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc == 0 && ble_hs_id_copy_addr(own_addr_type, addr_val, NULL) == 0) {
        ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
    }

    bleprph_advertise();
}

/* 蓝牙重置回调 */
static void bleprph_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

/* 蓝牙主机任务 */
static void bleprph_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化蓝牙
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;

    int rc = ble_svc_gap_device_name_set("MuLan");
    if (rc != 0) {
        ESP_LOGE(TAG, "set device name failed; rc=%d", rc);
    }

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed; rc=%d", rc);
        return;
    }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed; rc=%d", rc);
        return;
    }

    // 启动蓝牙主机任务
    nimble_port_freertos_init(bleprph_host_task);

    // 初始化显示
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);

    // lv_demo_music();
    // lv_demo_benchmark();
    // lv_demo_widgets();
    create_order_ui();

    bsp_display_unlock();
}
