#include "ble_lvgl_interface.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "BLE_LVGL";

void update_lvgl_with_ble_data(cJSON *json) {
    if (!json) return;

    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *age = cJSON_GetObjectItem(json, "age");
    cJSON *message = cJSON_GetObjectItem(json, "message");

    if (name && cJSON_IsString(name)) {
        lv_label_set_text(ble_name_label, name->valuestring);
    }
    if (age && cJSON_IsNumber(age)) {
        char age_str[16];
        snprintf(age_str, sizeof(age_str), "Age: %d", age->valueint);
        lv_label_set_text(ble_age_label, age_str);
    }
    if (message && cJSON_IsString(message)) {
        lv_label_set_text(ble_msg_label, message->valuestring);
    }
}

void create_ble_data_display(lv_obj_t *parent) {
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);

    ble_name_label = lv_label_create(panel);
    lv_label_set_text(ble_name_label, "Name: Waiting for data...");
    lv_obj_align(ble_name_label, LV_ALIGN_TOP_MID, 0, 20);

    ble_age_label = lv_label_create(panel);
    lv_label_set_text(ble_age_label, "Age: --");
    lv_obj_align(ble_age_label, LV_ALIGN_TOP_MID, 0, 60);

    ble_msg_label = lv_label_create(panel);
    lv_label_set_text(ble_msg_label, "Message: --");
    lv_obj_align(ble_msg_label, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "BLE Data Display");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, -20);
}