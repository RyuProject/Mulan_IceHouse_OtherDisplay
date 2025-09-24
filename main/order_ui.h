#ifndef ORDER_UI_H
#define ORDER_UI_H

#include "lvgl.h"

// 初始化订单UI容器（优化版）
void order_ui_init(lv_obj_t *parent);

// 清理订单UI资源
void order_ui_cleanup(void);

// 初始化菜品字体预渲染
void init_dish_font_prerender(void);

// 创建动态订单行
void create_dynamic_order_row(int order_num, const char *dishes);

// 创建带订单ID的动态订单行（优化版）
void create_dynamic_order_row_with_id(const char *order_id, int order_num, const char *dishes);

/**
 * @brief 显示弹出消息
 * 
 * @param message 要显示的消息内容
 * @param duration_ms 显示持续时间(毫秒)
 */
void show_popup_message(const char *message, uint32_t duration_ms);

/**
 * @brief 根据订单ID删除订单（优化版）
 * 
 * @param order_id 订单ID字符串
 */
void remove_order_by_id(const char *order_id);

/**
 * @brief 根据订单ID更新订单（优化版）
 * 
 * @param order_id 订单ID字符串
 * @param order_num 订单号
 * @param dishes 菜品内容
 */
void update_order_by_id(const char *order_id, int order_num, const char *dishes);

/**
 * @brief 发送蓝牙通知
 * 
 * @param json_str JSON格式的字符串
 * @return int 成功返回0，失败返回-1
 */
int send_notification(const char *json_str);

#endif // ORDER_UI_H