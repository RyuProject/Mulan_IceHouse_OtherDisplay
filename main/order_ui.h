#ifndef ORDER_UI_H
#define ORDER_UI_H

#include "lvgl.h"

// 初始化订单UI容器
void order_ui_init(lv_obj_t *parent);

// 创建动态订单行
void create_dynamic_order_row(int order_num, const char *dishes);

/**
 * @brief 显示弹出消息
 * 
 * @param message 要显示的消息内容
 * @param duration_ms 显示持续时间(毫秒)
 */
void show_popup_message(const char *message, uint32_t duration_ms);

#endif // ORDER_UI_H