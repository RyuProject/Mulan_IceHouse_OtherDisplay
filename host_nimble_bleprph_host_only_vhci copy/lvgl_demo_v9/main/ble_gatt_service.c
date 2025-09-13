#include "ble_lvgl_interface.h"
#include "esp_log.h"
#include "cJSON.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_GATT";

/* 16-bit UUID 自定义服务/特征 */
static ble_uuid16_t gatt_svc_uuid = BLE_UUID16_INIT(0xABCD);
static ble_uuid16_t gatt_chr_uuid = BLE_UUID16_INIT(0x1234);

static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_advertise(void);

/* GATT 特征访问回调 */
static int ble_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        uint8_t buf[512];
        uint16_t out_len = 0;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        
        if (out_len < sizeof(buf)) {
            buf[out_len] = '\0';
        } else {
            buf[sizeof(buf) - 1] = '\0';
            out_len = sizeof(buf) - 1;
        }
        
        ESP_LOGI(TAG, "Received data: %.*s", out_len, (char *)buf);
        
        cJSON *root = cJSON_Parse((char *)buf);
        if (root) {
            // 更新LVGL界面
            update_lvgl_with_ble_data(root);
            cJSON_Delete(root);
        }
        return 0;
    }
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
                .access_cb = ble_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_READ,
                .val_handle = 0,
            },
            {0}
        },
    },
    {0}
};

static void ble_on_sync(void)
{
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc == 0) {
        uint8_t addr_val[6];
        if (ble_hs_id_copy_addr(own_addr_type, addr_val, NULL) == 0) {
            ESP_LOGI(TAG, "Device Address: %02x:%02x:%02x:%02x:%02x:%02x",
                     addr_val[5], addr_val[4], addr_val[3],
                     addr_val[2], addr_val[1], addr_val[0]);
        }
    }
    ble_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void ble_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    const char *name = "MuLan";
    int rc;
    uint8_t own_addr_type;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer addr type failed; rc=%d", rc);
        return;
    }

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

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
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv start failed; rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started: %s", name);
}

void ble_gatt_service_init(void)
{
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;

    ble_svc_gap_device_name_set("MuLan");
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
}