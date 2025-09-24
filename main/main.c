
#include "font/lv_symbol_def.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lv_conf.h"  /* Include LVGL configuration first */
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
/* #include "lv_font_mulan_14.c" */
#include "../../managed_components/lvgl__lvgl/src/font/lv_font.h"
#include "lv_font_device.c"
#include "order_ui.h"
#include "hex_utils.h"
#include "utf8_validator.h"
#include <stdlib.h>

// 菜品字体预渲染函数声明
void init_dish_font_prerender(void);

// 添加字体文件系统支持
#if __has_include("esp_mmap_assets.h")
#include "esp_mmap_assets.h"
#include "esp_lv_fs.h"
#include "../../managed_components/lvgl__lvgl/src/font/lv_binfont_loader.h"
#include "mmap_generate_fonts.h"
#endif

static const char *TAG = "NimBLE_BLE_PRPH";

// 全局字体文件系统句柄
#if __has_include("esp_mmap_assets.h")
static mmap_assets_handle_t font_asset_handle = NULL;
static esp_lv_fs_handle_t fs_handle = NULL;
lv_font_t *dish_font = NULL;
lv_font_t *device_font = NULL;
lv_font_t *info_font = NULL;
#endif

// 字体加载任务函数声明
static void font_load_task_func(void *pvParameters);

/* Use LVGL built-in Montserrat font instead of missing mulan font */

static void create_order_ui(void)
{
    // lv_obj_t *scr = lv_scr_act();
    // lv_obj_set_style_bg_color(scr, lv_color_hex(0xf5f5f5), 0);
    
    // // 创建主容器，为底部状态栏留出空间
    // lv_obj_t *main_container = lv_obj_create(scr);
    // lv_obj_set_size(main_container, LV_PCT(100), LV_PCT(90));
    // lv_obj_set_style_border_width(main_container, 0, 0);
    // lv_obj_set_style_pad_all(main_container, 0, 0);
    
    // order_ui_init(main_container);
    
    // // 创建底部状态栏（按照Figma设计）
    // lv_obj_t *status_bar = lv_obj_create(scr);
    // lv_obj_set_size(status_bar, LV_PCT(100), 54);
    // lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    // lv_obj_set_style_bg_color(status_bar, lv_color_hex(0xF1F1F1), 0);
    // lv_obj_set_style_border_width(status_bar, 0, 0);
    // lv_obj_set_style_pad_all(status_bar, 0, 0);
    
    // // 电量显示
    // lv_obj_t *battery_label = lv_label_create(status_bar);
    // // lv_label_set_text(battery_label, "电量：100%");
    //     lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL "OK");
    // // lv_obj_set_style_text_font(battery_label, device_font, 0);
    // lv_obj_align(battery_label, LV_ALIGN_LEFT_MID, 18, 0);
    
    // // 蓝牙状态显示
    // lv_obj_t *bluetooth_label = lv_label_create(status_bar);
    // lv_label_set_text(bluetooth_label, LV_SYMBOL_BLUETOOTH "OK");
    // // lv_label_set_text(bluetooth_label, "蓝牙已连接");
    // // lv_obj_set_style_text_font(bluetooth_label, device_font, 0);
    // lv_obj_set_style_text_color(bluetooth_label, lv_color_hex(0x0CC160), 0);
    // lv_obj_align(bluetooth_label, LV_ALIGN_LEFT_MID, 181, 0);
}

static ble_uuid16_t gatt_svc_uuid = BLE_UUID16_INIT(0xABCD);
static ble_uuid16_t gatt_chr_uuid = BLE_UUID16_INIT(0x1234);
static ble_uuid16_t gatt_notify_uuid = BLE_UUID16_INIT(0x5678);
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t g_notify_handle = 0;

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static void bleprph_advertise(void);
static void bleprph_on_sync(void);
static void bleprph_on_reset(int reason);
static void bleprph_host_task(void *param);
int send_notification(const char *json_str)
{
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE || g_notify_handle == 0) {
        return -1;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(json_str, strlen(json_str));
    if (!om) {
        return -1;
    }

    int rc = ble_gattc_notify_custom(g_conn_handle, g_notify_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification: %d", rc);
        os_mbuf_free_chain(om);
        return rc;
    }

    return 0;
}

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
            {
                .uuid = (ble_uuid_t *)&gatt_notify_uuid,
                .access_cb = bleprph_chr_access,
                .flags = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
                .val_handle = &g_notify_handle,
            },
            {0}
        },
    },
    {0}
};

// 解码十六进制字符串到ASCII
static char* decode_hex_content(const char* hex_content, char* buffer, size_t buffer_size) {
    if (!hex_content || !buffer || buffer_size == 0) return NULL;
    
    int hex_len = strlen(hex_content);
    if (hex_len % 2 != 0 || !hex_is_valid(hex_content)) return NULL;
    
    int decoded_len = hex_to_ascii(hex_content, buffer, buffer_size);
    return decoded_len > 0 ? buffer : NULL;
}

// 处理系统消息
static void handle_system_message(cJSON* root) {
    cJSON *content = cJSON_GetObjectItem(root, "content");
    if (!content || !cJSON_IsString(content)) return;
    
    char *content_str = content->valuestring;
    char decoded_content[256] = {0};
    
    // 尝试解码十六进制内容
    if (decode_hex_content(content_str, decoded_content, sizeof(decoded_content))) {
        ESP_LOGI(TAG, "解码系统消息: %s", decoded_content);
        show_popup_message(decoded_content, 3000);
    } else {
        ESP_LOGI(TAG, "系统消息: %s", content_str);
        show_popup_message(content_str, 3000);
    }
}

// 构建菜品字符串（性能优化版）- 减少内存分配和字符串操作
static char* build_dishes_string(cJSON* items) {
    if (!items || !cJSON_IsArray(items)) return NULL;
    
    // 预计算所需内存大小
    size_t total_len = 1; // 终止符
    int item_count = 0;
    
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, items) {
        cJSON *name = cJSON_GetObjectItem(item, "name");
        if (!cJSON_IsString(name)) continue;
        
        char *name_str = name->valuestring;
        char decoded_name[128] = {0};
        
        // 尝试解码十六进制菜品名
        if (decode_hex_content(name_str, decoded_name, sizeof(decoded_name))) {
            name_str = decoded_name;
        }
        
        total_len += strlen(name_str);
        if (item_count > 0) total_len += 3; // "、"
        item_count++;
    }
    
    if (item_count == 0) return NULL;
    
    // 一次性分配足够内存
    char *dishes_str = malloc(total_len);
    if (!dishes_str) return NULL;
    
    dishes_str[0] = '\0';
    size_t current_pos = 0;
    item_count = 0;
    
    cJSON_ArrayForEach(item, items) {
        cJSON *name = cJSON_GetObjectItem(item, "name");
        if (!cJSON_IsString(name)) continue;
        
        char *name_str = name->valuestring;
        char decoded_name[128] = {0};
        
        if (decode_hex_content(name_str, decoded_name, sizeof(decoded_name))) {
            name_str = decoded_name;
        }
        
        size_t name_len = strlen(name_str);
        
        if (item_count > 0) {
            memcpy(dishes_str + current_pos, "、", 3);
            current_pos += 3;
        }
        
        memcpy(dishes_str + current_pos, name_str, name_len);
        current_pos += name_len;
        item_count++;
    }
    
    dishes_str[current_pos] = '\0';
    return dishes_str;
}

// 从订单ID生成订单号
static int generate_order_number(const char* order_id) {
    if (!order_id) return 1;
    
    int len = strlen(order_id);
    int order_num = 1;
    
    if (len > 4) {
        order_num = atoi(order_id + len - 4);
    } else {
        order_num = atoi(order_id);
    }
    
    return order_num > 0 ? order_num : 1;
}

// 蓝牙数据处理优化 - 减少JSON解析开销
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
        
        buf[out_len < sizeof(buf) ? out_len : sizeof(buf) - 1] = '\0';
        ESP_LOGI(TAG, "收到蓝牙JSON信息: %s", (char *)buf);
        
        // 快速检查消息类型，避免不必要的JSON解析
        const char *type_start = strstr((char *)buf, "\"type\"");
        if (!type_start) {
            // 尝试处理非标准JSON格式
            char *content_start = strstr((char *)buf, "content");
            if (content_start) {
                char *quote_start = strchr(content_start, '"');
                if (quote_start) {
                    char *quote_end = strchr(quote_start + 1, '"');
                    if (quote_end) {
                        *quote_end = '\0';
                        char *hex_content = quote_start + 1;
                        
                        char decoded_content[256] = {0};
                        if (decode_hex_content(hex_content, decoded_content, sizeof(decoded_content))) {
                            ESP_LOGW(TAG, "解码内容: %s", decoded_content);
                            show_popup_message(decoded_content, 3000);
                        }
                        *quote_end = '"';
                    }
                }
            }
            return BLE_ATT_ERR_UNLIKELY;
        }

        cJSON *root = cJSON_Parse((char *)buf);
        if (!root) {
            return BLE_ATT_ERR_UNLIKELY;
        }

        // 检查操作类型
        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (type && cJSON_IsString(type)) {
            const char *type_str = type->valuestring;
            
            if (strcmp(type_str, "info") == 0) {
                handle_system_message(root);
            } else if (strcmp(type_str, "add") == 0 || strcmp(type_str, "update") == 0 || strcmp(type_str, "remove") == 0) {
                bsp_display_lock(portMAX_DELAY);
                
                cJSON *id = cJSON_GetObjectItem(root, "orderId");
                if (!id || !cJSON_IsString(id)) {
                    ESP_LOGE(TAG, "无效的订单ID");
                    bsp_display_unlock();
                    cJSON_Delete(root);
                    return 0;
                }
                
                char *order_id = id->valuestring;
                ESP_LOGI(TAG, "处理订单: type=%s, orderId=%s", type_str, order_id);
                
                if (strcmp(type_str, "remove") == 0) {
                    remove_order_by_id(order_id);
                    show_popup_message("订单已删除", 2000);
                } else {
                    char *dishes_str = build_dishes_string(cJSON_GetObjectItem(root, "items"));
                    int order_num = generate_order_number(order_id);
                    
                    if (strcmp(type_str, "add") == 0) {
                        create_dynamic_order_row_with_id(order_id, order_num, dishes_str ? dishes_str : "无菜品");
                        show_popup_message("订单已添加", 2000);
                    } else {
                        update_order_by_id(order_id, order_num, dishes_str ? dishes_str : "无菜品");
                        show_popup_message("订单已更新", 2000);
                    }
                    
                    if (dishes_str) free(dishes_str);
                }
                
                bsp_display_unlock();
            }
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
            g_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Connected, handle=%d", event->connect.conn_handle);
        } else {
            ESP_LOGI(TAG, "Connect failed; status=%d", event->connect.status);
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
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

// 初始化字体文件系统（性能优化版）- 减少调试输出和延迟加载
static esp_err_t init_font_filesystem(void)
{
#if __has_include("esp_mmap_assets.h")
    const mmap_assets_config_t config = {
        .partition_label = "font",
        .max_files = MMAP_FONTS_FILES,
        .checksum = MMAP_FONTS_CHECKSUM,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = false,
        },
    };
    
    esp_err_t ret = mmap_assets_new(&config, &font_asset_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize font assets: %d", ret);
        return ret;
    }
    
    // 注册LVGL文件系统
    const fs_cfg_t fs_cfg = {
        .fs_letter = 'F',
        .fs_assets = font_asset_handle,
        .fs_nums = MMAP_FONTS_FILES
    };
    
    ret = esp_lv_fs_desc_init(&fs_cfg, &fs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LVGL filesystem: %d", ret);
        return ret;
    }
    
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

// 加载字体文件
static esp_err_t load_device_font(void)
{
    // 使用内置的设备字体
    device_font = &lv_font_device;
    ESP_LOGI(TAG, "Device font set to built-in lv_font_device");
    return ESP_OK;
}

static esp_err_t load_info_font(void)
{
    // 使用内置的设备字体
    info_font = &lv_font_device;
    ESP_LOGI(TAG, "Info font set to built-in lv_font_device");
    return ESP_OK;
}

static esp_err_t load_dish_font(void)
{
#if __has_include("esp_mmap_assets.h")
    dish_font = lv_binfont_create("F:lv_font_dishes.bin");
    if (!dish_font) {
        ESP_LOGE(TAG, "Failed to load dish font from lv_font_dishes.bin");
        // 设置fallback字体
        dish_font = &lv_font_device;
        ESP_LOGW(TAG, "Using fallback font for dish names");
        return ESP_OK; // 返回成功，因为fallback字体已设置
    }
    ESP_LOGI(TAG, "Dish font loaded successfully from lv_font_dishes.bin");
    return ESP_OK;
#else
    ESP_LOGW(TAG, "Font loading not available");
    // 设置fallback字体
    dish_font = &lv_font_device;
    return ESP_OK; // 返回成功，因为fallback字体已设置
#endif
}

// 字体加载任务函数
static void font_load_task_func(void *pvParameters) {
    load_dish_font();
    vTaskDelete(NULL);
}

// 全局字体缓存清理函数
void cleanup_font_cache(void) {
    // 清理LVGL字体缓存
#if __has_include("esp_mmap_assets.h")
    if (dish_font && dish_font != &lv_font_device) {
        lv_binfont_destroy(dish_font);
        dish_font = NULL;
    }
    // device_font 和 info_font 现在使用内置字体，不需要清理
#endif
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
    
    // 注册清理函数，确保程序退出时清理缓存
    atexit(cleanup_font_cache);

    // 初始化菜品字体预渲染（异步执行，不阻塞主线程）
    init_dish_font_prerender();

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

    // 优化显示配置 - 提高性能
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = {
            .task_priority = 6,     // 高优先级文字渲染
            .task_stack = 8192,      // 充足堆栈
            .task_affinity = -1,
            .task_max_sleep_ms = 2,  // 最小睡眠时间
            .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
            .timer_period_ms = 1     // 最小定时器周期
        },
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE * 4,  // 4倍缓冲区
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,        // DMA加速
            .buff_spiram = true,     // SPIRAM内存
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    // 异步初始化字体文件系统（减少启动时间）
    ret = init_font_filesystem();
    if (ret == ESP_OK) {
        // 快速加载基本字体，菜品字体延迟加载
        load_device_font();
        load_info_font();
        
        // 延迟加载菜品字体（在后台线程执行）
        static TaskHandle_t font_load_task = NULL;
        xTaskCreatePinnedToCore(
            font_load_task_func,
            "font_load",
            4096,
            NULL,
            1,
            &font_load_task,
            0
        );
    }
    
    // 最小化显示锁定时间
    bsp_display_lock(portMAX_DELAY);
    order_ui_init(lv_scr_act());
    bsp_display_unlock();
}
