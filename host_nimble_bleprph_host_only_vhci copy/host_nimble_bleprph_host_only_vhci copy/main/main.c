#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"          // ble_hs_util_ensure_addr, ble_hs_id_infer_auto
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"

static const char *TAG = "NimBLE_BLE_PRPH";

/* 16-bit UUID 自定义服务/特征 */
static ble_uuid16_t gatt_svc_uuid = BLE_UUID16_INIT(0xABCD);
static ble_uuid16_t gatt_chr_uuid = BLE_UUID16_INIT(0x1234);

/* 前置声明 */
static int  bleprph_gap_event(struct ble_gap_event *event, void *arg);
static void bleprph_advertise(void);
static void bleprph_on_sync(void);
static void bleprph_on_reset(int reason);
static void bleprph_host_task(void *param);

void ble_store_config_init(void);

/* 可选：注册回调，便于打印已注册的 GATT 资源 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "registered service handle=%d", ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG, "registered char def=%d val=%d", ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "registered desc handle=%d", ctxt->dsc.handle);
        break;
    default:
        break;
    }
}

/* GATT 特征访问回调：安全读取 mbuf 数据 */
static int bleprph_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint8_t buf[512]; // 增大缓冲区以适应较大的JSON数据
        uint16_t out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        
        // 确保字符串以null结尾
        if (out_len < sizeof(buf)) {
            buf[out_len] = '\0';
        } else {
            buf[sizeof(buf) - 1] = '\0';
            out_len = sizeof(buf) - 1;
        }
        
        ESP_LOGI(TAG, "Received data: %.*s", out_len, (char *)buf);
        
        // 尝试解析JSON数据
        cJSON *root = cJSON_Parse((char *)buf);
        if (root) {
            ESP_LOGI(TAG, "成功解析JSON数据");
            
            // 示例：解析JSON中的特定字段
            cJSON *name = cJSON_GetObjectItem(root, "name");
            if (name && cJSON_IsString(name)) {
                ESP_LOGI(TAG, "name: %s", name->valuestring);
            }
            
            cJSON *age = cJSON_GetObjectItem(root, "age");
            if (age && cJSON_IsNumber(age)) {
                ESP_LOGI(TAG, "age: %d", age->valueint);
            }
            
            cJSON *message = cJSON_GetObjectItem(root, "message");
            if (message && cJSON_IsString(message)) {
                ESP_LOGI(TAG, "message: %s", message->valuestring);
            }
            
            // 打印完整的JSON结构
            char *json_str = cJSON_Print(root);
            if (json_str) {
                ESP_LOGI(TAG, "JSON结构: %s", json_str);
                free(json_str);
            }
            
            // 释放cJSON对象
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "JSON解析失败: %s", cJSON_GetErrorPtr());
        }
        
        return 0;
    }
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ESP_LOGI(TAG, "Characteristic read request");
        return 0;
    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* 自定义 GATT 服务定义 */
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t *)&gatt_svc_uuid,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = (ble_uuid_t *)&gatt_chr_uuid,
                .access_cb = bleprph_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                .val_handle = 0,   /* 让协议栈自动分配 */
            },
            {0}
        },
    },
    {0}
};

static int bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected, handle=%d", event->connect.conn_handle);
        } else {
            ESP_LOGI(TAG, "Connect failed; status=%d", event->connect.status);
            vTaskDelay(pdMS_TO_TICKS(1000));
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
        vTaskDelay(pdMS_TO_TICKS(1000));
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete");
        vTaskDelay(pdMS_TO_TICKS(1000));
        bleprph_advertise();
        return 0;

    default:
        return 0;
    }
}

static void bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    const char *name = "MuLan";
    int rc;
    uint8_t own_addr_type;

    /* 按示例流程推断地址类型并确保本机地址可用 */
    ble_hs_util_ensure_addr(0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer addr type failed; rc=%d", rc);
        return;
    }

    /* 广播数据（短字段） */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    /* 扫描响应：完整名 + 16-bit 服务 UUID */
    rsp_fields.name = (uint8_t *)name;
    rsp_fields.name_len = strlen(name);
    rsp_fields.name_is_complete = 1;
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

static void bleprph_on_sync(void)
{
    uint8_t own_addr_type;
    uint8_t addr_val[6];
    int rc;

    /* 与教程一致：先确保地址，再打印，再开始广播 */
    ble_hs_util_ensure_addr(0);
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc == 0 && ble_hs_id_copy_addr(own_addr_type, addr_val, NULL) == 0) {
        ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
                 addr_val[5], addr_val[4], addr_val[3],
                 addr_val[2], addr_val[1], addr_val[0]);
    }

    bleprph_advertise();
}

static void bleprph_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void bleprph_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    int rc;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return;
    }

    /* 初始化 GAP/GATT 基础服务（示例推荐） */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* 配置 host 回调与存储（示例均调用 ble_store_config_init） */
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    /* 设置设备名（示例同流程） */
    rc = ble_svc_gap_device_name_set("MuLan");
    if (rc != 0) {
        ESP_LOGE(TAG, "set device name failed; rc=%d", rc);
    }

    /* 注册自定义 GATT 服务（host 启动前） */
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

    /* 配置安全材料存储回调（文档要求调用） */
    ble_store_config_init();  // 按示例放置在 host 启动前 (见文档中的 “XXX Need to have template for store” 段落)

    /* 启动 NimBLE Host 任务 */
    nimble_port_freertos_init(bleprph_host_task);
    ESP_LOGI(TAG, "BLE example started");
}