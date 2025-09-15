
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
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
#include "order_ui.h"
#include "hex_utils.h"
#include "utf8_validator.h"
#include <stdlib.h>

static const char *TAG = "NimBLE_BLE_PRPH";

LV_FONT_DECLARE(lv_font_mulan_14);

static void create_order_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xf5f5f5), 0);
    order_ui_init(scr);
}

static ble_uuid16_t gatt_svc_uuid = BLE_UUID16_INIT(0xABCD);
static ble_uuid16_t gatt_chr_uuid = BLE_UUID16_INIT(0x1234);

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static void bleprph_advertise(void);
static void bleprph_on_sync(void);
static void bleprph_on_reset(int reason);
static void bleprph_host_task(void *param);
static int bleprph_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);

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

static int bleprph_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint8_t buf[512];
        uint16_t out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len);
        if (rc != 0) {
            ESP_LOGE(TAG, "ble_hs_mbuf_to_flat failed: %d", rc);
            return BLE_ATT_ERR_UNLIKELY;
        }
        
        if (out_len < sizeof(buf)) buf[out_len] = '\0';
        else buf[sizeof(buf) - 1] = '\0';
        
        cJSON *root = cJSON_Parse((char *)buf);
        if (!root) {
            if (strstr((char *)buf, "content") && strstr((char *)buf, "\"") && (strstr((char *)buf, "type") || strstr((char *)buf, "_id"))) {
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
                            if (hex_len % 2 == 0 && hex_is_valid(hex_content)) {
                                ESP_LOGI(TAG, "Hex content detected: %s", hex_content);
                                
                                // 解码十六进制
                                char decoded_content[256] = {0};
                                int decoded_len = hex_to_ascii(hex_content, decoded_content, sizeof(decoded_content));
                                
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
                if (hex_len % 2 == 0 && hex_is_valid(content_str)) {
                    ESP_LOGI(TAG, "检测到十六进制编码内容: %s", content_str);
                    
                    // 解码十六进制
                    decoded_len = hex_to_ascii(content_str, decoded_content, sizeof(decoded_content));
                    
                    if (decoded_len > 0) {
                        ESP_LOGI(TAG, "解码后的内容: %s", decoded_content);
                        
                        // 使用解码后的内容
                        content_str = decoded_content;
                    }
                }
                
                // 使用模块化的弹窗函数（黑色背景，3秒后自动消失）
                show_popup_message(decoded_len > 0 ? decoded_content : content_str, 3000);
            }
        }

        if (is_order) {
            // 处理订单数据
            // 加锁保护UI更新
            bsp_display_lock(portMAX_DELAY);
            
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
                        
                        if (hex_len % 2 == 0 && hex_is_valid(name_str)) {
                            ESP_LOGI(TAG, "检测到十六进制编码的菜品名: %s", name_str);
                            int decoded_len = hex_to_ascii(name_str, decoded_name, sizeof(decoded_name));
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
            
            // 创建动态订单行（使用模块化接口）
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

    nimble_port_freertos_init(bleprph_host_task);

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
    order_ui_init(lv_scr_act());
    bsp_display_unlock();
}
