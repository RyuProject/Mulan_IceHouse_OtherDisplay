#include "lvgl.h"
#include "order_ui.h"
#include "esp_log.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "sys/queue.h"
#include "cJSON.h"
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"

// 外部声明send_notification函数
extern int send_notification(const char *json_str);

// 外部声明字体
/* extern lv_font_t lv_font_mulan_14; */
extern lv_font_t lv_font_dishes_24;
extern lv_font_t lv_font_device_24;
LV_FONT_DECLARE(lv_font_montserrat_14)
LV_FONT_DECLARE(lv_font_simsun_16_cjk)

static const char *TAG = "OrderUI";
static lv_obj_t *orders_container = NULL;
static lv_obj_t *waiting_label = NULL; // 保存等待标签指针
static lv_obj_t *bluetooth_label = NULL; // 保存蓝牙状态标签指针
static lv_timer_t *bluetooth_timer = NULL; // 蓝牙状态更新定时器

// 检查蓝牙连接状态
static bool is_bluetooth_connected(void) {
    extern uint16_t g_conn_handle;
    return g_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

// 更新蓝牙状态显示
static void update_bluetooth_status(void) {
    if (!bluetooth_label || !lv_obj_is_valid(bluetooth_label)) return;
    
    bsp_display_lock(portMAX_DELAY);
    if (is_bluetooth_connected()) {
        lv_label_set_text(bluetooth_label, LV_SYMBOL_BLUETOOTH "OK");
        lv_obj_set_style_text_color(bluetooth_label, lv_color_hex(0x0CC160), 0); // 绿色
    } else {
        lv_label_set_text(bluetooth_label, LV_SYMBOL_BLUETOOTH "READY");
        lv_obj_set_style_text_color(bluetooth_label, lv_color_hex(0xFF9500), 0); // 橙色
    }
    bsp_display_unlock();
}

// 蓝牙状态定时器回调
static void bluetooth_timer_cb(lv_timer_t *timer) {
    if (!timer) return;
    update_bluetooth_status();
}

// 订单状态枚举
typedef enum {
    ORDER_STATUS_PENDING,    // 等待中
    ORDER_STATUS_COMPLETED,  // 已完成
    ORDER_STATUS_REMOVED     // 已移除
} order_status_t;

// 订单数据结构
typedef struct order_info {
    char *order_id;          // 订单ID
    int order_num;           // 订单号
    char *dishes;            // 菜品信息
    lv_obj_t *row_widget;    // 订单行UI对象
    lv_obj_t *dish_label;   // 菜品标签指针
    order_status_t status;   // 订单状态
    STAILQ_ENTRY(order_info) entries;
} order_info_t;

// 订单队列
STAILQ_HEAD(order_list_head, order_info);
static struct order_list_head order_list = STAILQ_HEAD_INITIALIZER(order_list);

// 按钮点击事件：已出餐 → 修改按钮状态、文字、颜色
// 通知特性UUID
#define NOTIFY_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static void btn_ready_cb(lv_event_t *e)
{
    bsp_display_lock(portMAX_DELAY);
    
    lv_obj_t *btn = lv_event_get_target(e);
    
    // 获取订单行和订单信息
    lv_obj_t *row = lv_obj_get_parent(btn);
    if (row) {
        order_info_t *order = NULL;
        STAILQ_FOREACH(order, &order_list, entries) {
            if (order->row_widget == row) {
                // 删除出餐按钮
                lv_obj_del(btn);
                
                // 将订单行背景色改为灰色
                lv_obj_set_style_bg_color(row, lv_color_hex(0xCCCCCC), 0);
                
                // 更新订单状态为已完成
                order->status = ORDER_STATUS_COMPLETED;
                
                // 构建JSON通知消息
                char notify_msg[128];
                snprintf(notify_msg, sizeof(notify_msg), 
                        "{\"orderId\":\"%s\",\"status\":true}", 
                        order->order_id);
                
                // 通过蓝牙通知发送
                send_notification(notify_msg);
                
                ESP_LOGI(TAG, "已发送出餐通知: %s", notify_msg);
                break;
            }
        }
    }

    ESP_LOGI(TAG, "✅ 已出餐按钮被点击并已移除");
    bsp_display_unlock();
}

// 初始化订单UI容器
void order_ui_init(lv_obj_t *parent)
{
    bsp_display_lock(portMAX_DELAY);
    
    orders_container = lv_obj_create(parent);
    lv_obj_set_size(orders_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(orders_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(orders_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(orders_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(orders_container, 10, 0);

    // 初始显示等待数据 - 使用混合字体
    lv_obj_t *waiting_container = lv_obj_create(orders_container);
    lv_obj_set_size(waiting_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(waiting_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(waiting_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(waiting_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(waiting_container, 0, 0);
    lv_obj_set_style_pad_all(waiting_container, 0, 0);
    lv_obj_center(waiting_container);
    
    // 中文部分使用设备字体（优化显示）
    lv_obj_t *text_label = lv_label_create(waiting_container);
    lv_obj_set_style_text_font(text_label, &lv_font_device_24, 0);
    lv_obj_set_style_text_color(text_label, lv_color_hex(0x2C2C2C), 0); // 深色提高可读性
    lv_label_set_text(text_label, "等待蓝牙连接");
    
    // 省略号部分使用Montserrat字体（优化对齐）
    lv_obj_t *dots_label = lv_label_create(waiting_container);
    lv_obj_set_style_text_font(dots_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(dots_label, lv_color_hex(0x666666), 0); // 灰色
    lv_obj_set_style_pad_top(dots_label, 2, 0); // 垂直居中微调
    lv_label_set_text(dots_label, "...");
    
    waiting_label = waiting_container;

    // 创建底部状态栏（按照Figma设计）
    lv_obj_t *status_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(status_bar, LV_PCT(100), 40);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0xF1F1F1), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    
    lv_obj_t *bar_label = lv_label_create(status_bar);
    lv_label_set_text(bar_label, "MuLanKDS ver0.1");
    lv_obj_align(bar_label, LV_ALIGN_LEFT_MID, 18, 0);

    // 电量显示
    lv_obj_t *battery_label = lv_label_create(status_bar);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL "OK");
    lv_obj_align(battery_label, LV_ALIGN_LEFT_MID, 170, 0);
    
    // 蓝牙状态显示
    bluetooth_label = lv_label_create(status_bar);
    update_bluetooth_status(); // 初始设置状态
    lv_obj_align(bluetooth_label, LV_ALIGN_LEFT_MID, 230, 0);

    // 创建蓝牙状态更新定时器（每秒更新一次）
    bluetooth_timer = lv_timer_create(bluetooth_timer_cb, 1000, NULL);
    if (bluetooth_timer) {
        lv_timer_set_repeat_count(bluetooth_timer, -1); // 无限重复
    }

    bsp_display_unlock();
}



/* 弹出窗口定时器回调 */
static void popup_timer_cb(lv_timer_t *timer) {
    if (!timer) return;
    
    lv_obj_t *popup = (lv_obj_t *)lv_timer_get_user_data(timer);
    
    // 仅在需要访问LVGL对象时加锁
    if (popup && lv_obj_is_valid(popup)) {
        bsp_display_lock(portMAX_DELAY);
        if (lv_obj_is_valid(popup)) {
            lv_obj_del(popup);
        }
        bsp_display_unlock();
    }
    
    lv_timer_del(timer); // 最后删除定时器，且只删一次
}

void show_popup_message(const char *message, uint32_t duration_ms) {
    bsp_display_lock(portMAX_DELAY);
    
    // 创建弹窗
    lv_obj_t *popup = lv_obj_create(lv_scr_act());
    if (!popup) {
        bsp_display_unlock();
        ESP_LOGE(TAG, "创建弹窗失败");
        return;
    }

    // 基本样式设置
    lv_obj_set_size(popup, 280, 80);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(popup, 0, 0);
    lv_obj_set_style_pad_all(popup, 5, 0);
    lv_obj_set_style_text_font(popup, &lv_font_montserrat_14, 0);

    // 创建消息标签
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    // 设置定时器自动关闭弹出窗口
    lv_timer_t *timer = lv_timer_create(popup_timer_cb, duration_ms, popup);
    if (!timer) {
        // 定时器创建失败时手动删除弹窗
        lv_obj_del(popup);
        bsp_display_unlock();
        ESP_LOGE(TAG, "创建弹窗定时器失败");
        return;
    }
    lv_timer_set_repeat_count(timer, 1);

    bsp_display_unlock();
}

// 根据订单ID查找订单
static order_info_t *find_order_by_id(const char *order_id) {
    order_info_t *order;
    STAILQ_FOREACH(order, &order_list, entries) {
        if (order->order_id && strcmp(order->order_id, order_id) == 0) {
            return order;
        }
    }
    return NULL;
}

// Unicode十六进制编码解码函数
static char* decode_unicode_hex(const char* hex_str) {
    if (!hex_str) return NULL;
    
    size_t len = strlen(hex_str);
    if (len % 2 != 0) return strdup(hex_str); // 非有效的十六进制字符串
    
    size_t out_len = len / 2 + 1;
    char* result = malloc(out_len);
    if (!result) return NULL;
    
    for (size_t i = 0, j = 0; i < len; i += 2, j++) {
        char hex[3] = {hex_str[i], hex_str[i+1], '\0'};
        int value;
        sscanf(hex, "%x", &value);
        result[j] = (char)value;
    }
    result[out_len - 1] = '\0';
    
    return result;
}

// 高性能菜品卡片创建（优化中文显示）
static lv_obj_t* create_dish_card(lv_obj_t* parent, const char* dish_name) {
    lv_obj_t *dish_label = lv_label_create(parent);
    if (!dish_label) return NULL;
    
    lv_label_set_text(dish_label, dish_name);
    lv_obj_set_style_text_font(dish_label, &lv_font_dishes_24, 0);
    lv_obj_set_style_text_color(dish_label, lv_color_hex(0x333333), 0); // 深灰色提高可读性
    lv_obj_set_style_pad_all(dish_label, 8, 0);
    lv_obj_set_style_margin_all(dish_label, 5, 0);
    lv_obj_set_style_bg_color(dish_label, lv_color_hex(0xF8F8F8), 0); // 更浅的背景色
    lv_obj_set_style_radius(dish_label, 6, 0); // 稍微增加圆角
    lv_obj_set_style_border_width(dish_label, 0, 0);
    lv_obj_set_style_text_opa(dish_label, LV_OPA_COVER, 0);
    
    return dish_label;
}

// 创建订单行（带订单ID）- 优化版
void create_dynamic_order_row_with_id(const char *order_id, int order_num, const char *dishes) {
    bsp_display_lock(portMAX_DELAY);
    
    // 清理等待标签
    if (waiting_label && lv_obj_is_valid(waiting_label)) {
        lv_obj_del(waiting_label);
        waiting_label = NULL;
    }

    // 创建订单行
    lv_obj_t *row = lv_obj_create(orders_container);
    if (!row) {
        bsp_display_unlock();
        ESP_LOGE(TAG, "创建订单行失败");
        return;
    }
    
    // 分配订单内存
    order_info_t *order = malloc(sizeof(order_info_t));
    if (!order) {
        lv_obj_del(row);
        bsp_display_unlock();
        ESP_LOGE(TAG, "分配订单内存失败");
        return;
    }
    
    memset(order, 0, sizeof(order_info_t));
    order->order_id = strdup(order_id);
    order->dishes = strdup(dishes);
    order->order_num = order_num;
    order->row_widget = row;
    order->status = ORDER_STATUS_PENDING;
    
    if (!order->order_id || !order->dishes) {
        if (order->order_id) free(order->order_id);
        if (order->dishes) free(order->dishes);
        free(order);
        lv_obj_del(row);
        bsp_display_unlock();
        ESP_LOGE(TAG, "复制订单信息失败");
        return;
    }

    // 高性能订单行样式（简化）
    lv_obj_set_size(row, LV_PCT(100), 80);  // 减少高度节省内存
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_all(row, 8, 0);    // 减少内边距
    lv_obj_set_style_radius(row, 3, 0);    // 简化圆角
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_to_index(row, 0);

    // 左侧：菜品容器（简化版）
    lv_obj_t *left_container = lv_obj_create(row);
    if (!left_container) {
        free(order->dishes);
        free(order->order_id);
        free(order);
        lv_obj_del(row);
        bsp_display_unlock();
        ESP_LOGE(TAG, "创建左侧容器失败");
        return;
    }
    
    lv_obj_set_width(left_container, LV_PCT(70));
    lv_obj_set_height(left_container, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_border_width(left_container, 0, 0);
    lv_obj_set_style_pad_all(left_container, 0, 0);
    lv_obj_set_style_bg_opa(left_container, LV_OPA_TRANSP, 0); // 透明背景减少渲染
    
    // 高性能菜品解析（减少日志和内存分配）
    cJSON *root = cJSON_Parse(dishes);
    if (root) {
        cJSON *items = cJSON_GetObjectItem(root, "items");
        if (items && cJSON_IsArray(items)) {
            cJSON *item = NULL;
            cJSON_ArrayForEach(item, items) {
                cJSON *name = cJSON_GetObjectItem(item, "name");
                if (name && cJSON_IsString(name)) {
                    // 直接使用名称，避免不必要的解码和日志
                    create_dish_card(left_container, name->valuestring);
                }
            }
        }
        cJSON_Delete(root);
    } else {
        // 简单字符串直接显示
        create_dish_card(left_container, dishes);
    }
    
    order->dish_label = left_container;

    // 右侧：创建真正的按钮对象
    lv_obj_t *btn_ready = lv_btn_create(row);
    if (!btn_ready) {
        free(order->dishes);
        free(order->order_id);
        free(order);
        lv_obj_del(left_container);
        lv_obj_del(row);
        bsp_display_unlock();
        ESP_LOGE(TAG, "创建按钮失败");
        return;
    }
    
    lv_obj_set_size(btn_ready, 100, 60);
    lv_obj_align(btn_ready, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_ready, lv_color_hex(0x4CAF50), 0); // 更标准的绿色
    lv_obj_add_event_cb(btn_ready, btn_ready_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建按钮标签
    lv_obj_t *btn_label = lv_label_create(btn_ready);
    lv_label_set_text(btn_label, "出餐");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_device_24, 0);
    lv_obj_center(btn_label);

    STAILQ_INSERT_TAIL(&order_list, order, entries);
    bsp_display_unlock();
}

// 创建订单行（动态添加到 UI）
void create_dynamic_order_row(int order_num, const char *dishes) {
    // 默认使用订单号作为ID
    char default_id[32];
    snprintf(default_id, sizeof(default_id), "order_%d", order_num);
    create_dynamic_order_row_with_id(default_id, order_num, dishes);
}

// 根据订单ID删除订单
void remove_order_by_id(const char *order_id) {
    bsp_display_lock(portMAX_DELAY);

    order_info_t *order = find_order_by_id(order_id);
    if (!order) {
        bsp_display_unlock();
        ESP_LOGW(TAG, "订单ID %s 不存在，无法删除", order_id);
        return;
    }

    // 从UI中移除订单行
    if (order->row_widget && lv_obj_is_valid(order->row_widget)) {
        lv_obj_del(order->row_widget);
        order->row_widget = NULL;
    }

    // 从队列中移除并释放内存
    STAILQ_REMOVE(&order_list, order, order_info, entries);
    free(order->order_id);
    free(order->dishes);
    free(order);

    // 如果删除后队列为空，恢复等待标签
    if (STAILQ_EMPTY(&order_list) && orders_container && lv_obj_is_valid(orders_container) && !waiting_label) {
        // 创建已连接状态标签（优化显示）
        waiting_label = lv_label_create(orders_container);
        lv_obj_set_style_text_font(waiting_label, &lv_font_device_24, 0);
        lv_obj_set_style_text_color(waiting_label, lv_color_hex(0x4CAF50), 0); // 绿色表示已连接
        lv_label_set_text(waiting_label, "已连接");
        lv_obj_center(waiting_label);
        lv_obj_center(waiting_label);
    }

    bsp_display_unlock();
    ESP_LOGI(TAG, "已删除订单: %s", order_id);
}

// 根据订单ID更新订单信息
void update_order_by_id(const char *order_id, int order_num, const char *dishes) {
    bsp_display_lock(portMAX_DELAY);

    order_info_t *order = find_order_by_id(order_id);
    if (!order) {
        bsp_display_unlock();
        ESP_LOGW(TAG, "订单ID %s 不存在，无法更新", order_id);
        return;
    }

    // 更新订单信息
    order->order_num = order_num;
    
    // 安全更新菜品信息
    char *new_dishes = strdup(dishes);
    if (new_dishes) {
        free(order->dishes);
        order->dishes = new_dishes;
    }
    
    // 高性能UI更新 - 直接更新菜品标签内容
    if (order->dish_label && lv_obj_is_valid(order->dish_label)) {
        // 清除所有子对象（菜品卡片）
        lv_obj_clean(order->dish_label);
        
        // 简单解析并显示菜品
        cJSON *root = cJSON_Parse(dishes);
        if (root) {
            cJSON *items = cJSON_GetObjectItem(root, "items");
            if (items && cJSON_IsArray(items)) {
                cJSON *item = NULL;
                cJSON_ArrayForEach(item, items) {
                    cJSON *name = cJSON_GetObjectItem(item, "name");
                    if (name && cJSON_IsString(name)) {
                        create_dish_card(order->dish_label, name->valuestring);
                    }
                }
            }
            cJSON_Delete(root);
        } else {
            // 简单字符串直接显示
            create_dish_card(order->dish_label, dishes);
        }
    }
    
    bsp_display_unlock();
}