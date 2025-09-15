#ifndef ORDER_UI_H
#define ORDER_UI_H

#include "lvgl.h"

// 初始化订单UI容器
void order_ui_init(lv_obj_t *parent);

// 创建动态订单行
void create_dynamic_order_row(int order_num, const char *dishes);

#endif // ORDER_UI_H