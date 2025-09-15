#include "order_ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include "bsp/display.h"

// 外部声明字体
extern lv_font_t lv_font_mulan_14;

static const char *TAG = "OrderUI";
static lv_obj_t *orders_container = NULL;

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

// 初始化订单UI容器
void order_ui_init(lv_obj_t *parent)
{
    orders_container = lv_obj_create(parent);
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

// 创建订单行（动态添加到 UI）
void create_dynamic_order_row(int order_num, const char *dishes) {
    lv_obj_t *row = lv_obj_create(orders_container);
    lv_obj_set_size(row, LV_PCT(100), 80);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_obj_set_style_pad_all(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // 将新订单行移动到容器顶部
    lv_obj_move_to_index(row, 0);

    // 左侧：订单信息（垂直）
    lv_obj_t *left_container = lv_obj_create(row);
    lv_obj_set_flex_flow(left_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_width(left_container, LV_SIZE_CONTENT);
                    lv_obj_set_style_border_width(left_container, 0, 0);
    lv_obj_set_style_pad_all(left_container, 5, 0);

    lv_obj_t *dish_label = lv_label_create(left_container);
    lv_label_set_text_fmt(dish_label, "%s", dishes);
    lv_obj_set_style_text_font(dish_label, &lv_font_mulan_14, 0);
    lv_obj_align(dish_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // 右侧：已出餐按钮
    lv_obj_t *btn_ready = lv_btn_create(row);
    lv_obj_set_size(btn_ready, 80, 30);
    lv_obj_align(btn_ready, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_bg_color(btn_ready, lv_color_hex(0x007AFF), 0);  // 初始为蓝色
    lv_obj_set_style_radius(btn_ready, 4, 0);
    lv_obj_clear_flag(btn_ready, LV_OBJ_FLAG_SCROLLABLE);

    // 按钮文字
    lv_obj_t *btn_label = lv_label_create(btn_ready);
    lv_label_set_text(btn_label, "已出餐");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);

    // 添加点击事件
    lv_obj_add_event_cb(btn_ready, btn_ready_cb, LV_EVENT_CLICKED, NULL);
}