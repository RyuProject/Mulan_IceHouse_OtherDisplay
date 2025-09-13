#pragma once
#include "lvgl.h"
#include "cJSON.h"

typedef struct {
    char name[32];
    int age;
    char message[128];
} ble_data_t;

void update_lvgl_with_ble_data(cJSON *json);
void create_ble_data_display(lv_obj_t *parent);