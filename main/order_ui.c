#include "order_ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"

// 外部声明字体
extern lv_font_t lv_font_mulan_14;

static const char *TAG = "OrderUI";
static lv_obj_t *orders_container = NULL;

// 按钮点击事件：已出餐 → 修改按钮状态、文字、颜色
static void btn_ready_cb(lv_event_t *e)
{
    bsp_display_lock(portMAX_DELAY);
    
    lv_obj_t *btn = lv_event_get_target(e);
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

    bsp_display_unlock();
    
    ESP_LOGI(TAG, "✅ 已出餐按钮被点击并已禁用");
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

    // 初始显示等待数据
    lv_obj_t *waiting_label = lv_label_create(orders_container);
    lv_obj_set_style_text_font(waiting_label, &lv_font_mulan_14, 0);
    lv_label_set_text(waiting_label, "等待订单数据...");
    lv_obj_center(waiting_label);

    bsp_display_unlock();
}

/**
 * @brief 显示弹出消息
 * 
 * @param message 要显示的消息内容
 * @param duration_ms 显示持续时间(毫秒)
 */
/* 弹出窗口定时器回调 */
static void popup_timer_cb(lv_timer_t *timer) {
    if (timer) {
        lv_obj_t *popup = (lv_obj_t *)lv_timer_get_user_data(timer);
        if (popup && lv_obj_is_valid(popup)) {
            bsp_display_lock(portMAX_DELAY);
            lv_obj_del(popup);
            bsp_display_unlock();
        }
        lv_timer_del(timer);
    }
}

void show_popup_message(const char *message, uint32_t duration_ms) {
    // 确保在主线程执行UI操作
    if (xPortGetFreeHeapSize() < 2048) {
        ESP_LOGE(TAG, "内存不足，无法创建弹窗");
        return;
    }

    bsp_display_lock(portMAX_DELAY);
    
    // 创建更简单的弹窗
    lv_obj_t *popup = lv_obj_create(lv_scr_act());
    if (!popup) {
        bsp_display_unlock();
        return;
    }

    // 基本样式设置
    lv_obj_set_size(popup, 280, 80);
    lv_obj_center(popup);
    lv_obj_set_style_bg_color(popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(popup, 0, 0);
    lv_obj_set_style_pad_all(popup, 5, 0);
    lv_obj_set_style_text_font(popup, &lv_font_mulan_14, 0);

    // 创建消息标签
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    // 设置定时器自动关闭弹出窗口
    lv_timer_t *timer = lv_timer_create(popup_timer_cb, duration_ms, popup);
    if (timer) {
        lv_timer_set_repeat_count(timer, 1);
    }

    bsp_display_unlock();
}

// 创建订单行（动态添加到 UI）
void create_dynamic_order_row(int order_num, const char *dishes) {
    bsp_display_lock(portMAX_DELAY);
    
    // 首次添加订单时，清理"等待订单数据..."占位标签
    uint32_t child_cnt = lv_obj_get_child_cnt(orders_container);
    if (child_cnt == 1) {
        lv_obj_t *child = lv_obj_get_child(orders_container, 0);
        // 判断是否为等待数据的标签
        if (child && lv_obj_check_type(child, &lv_label_class)) {
            lv_obj_del(child);
        }
    }

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

    bsp_display_unlock();
}