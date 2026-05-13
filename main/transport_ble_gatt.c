#include "transport_ble_gatt.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "sdkconfig.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define BLE_GATT_DEFAULT_MTU 23U

static const char *TAG = "BLE_GATT";

static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_status_char_handle;
static uint16_t s_rule_event_char_handle;
static uint16_t s_mtu = BLE_GATT_DEFAULT_MTU;
static bool s_connected;
static bool s_status_notify_enabled;
static bool s_rule_event_notify_enabled;
static bool s_started;
static transport_ble_status_snapshot_t s_status_snapshot;

static const ble_uuid16_t s_service_uuid = BLE_UUID16_INIT(BLE_RULE_SERVICE_UUID);
static const ble_uuid16_t s_status_char_uuid = BLE_UUID16_INIT(BLE_RULE_STATUS_CHAR_UUID);
static const ble_uuid16_t s_rule_event_char_uuid = BLE_UUID16_INIT(BLE_RULE_EVENT_CHAR_UUID);

static ble_rule_status_packet_t make_status_packet(void)
{
    ble_rule_status_packet_t packet = {
        .magic = BLE_RULE_STATUS_MAGIC,
        .version = BLE_RULE_STATUS_VERSION,
        .packet_bytes = sizeof(ble_rule_status_packet_t),
        .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .app_mode = (uint8_t)s_status_snapshot.app_mode,
        .ble_connected = s_connected ? 1 : 0,
        .status_notify_enabled = s_status_notify_enabled ? 1 : 0,
        .rule_event_notify_enabled = s_rule_event_notify_enabled ? 1 : 0,
    };
    return packet;
}

static int gatt_status_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }
    ble_rule_status_packet_t packet = make_status_packet();
    int rc = os_mbuf_append(ctxt->om, &packet, sizeof(packet));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static ble_rule_event_packet_t make_rule_event_packet(const rule_event_t *event)
{
    ble_rule_event_packet_t packet = {
        .magic = BLE_RULE_EVENT_MAGIC,
        .version = BLE_RULE_EVENT_VERSION,
        .packet_bytes = sizeof(ble_rule_event_packet_t),
    };
    if (event != NULL) {
        packet.sequence = event->sequence;
        packet.uptime_ms = event->uptime_ms;
        packet.rule_id = event->rule_id;
        packet.source = (uint16_t)event->source;
        packet.action = (uint16_t)event->action;
        packet.fire_count = event->fire_count;
        if (event->measured_value.kind == RULE_VALUE_BOOL) {
            packet.measured_i32 = event->measured_value.as.bool_value ? 1 : 0;
        } else if (event->measured_value.kind == RULE_VALUE_I32) {
            packet.measured_i32 = event->measured_value.as.i32_value;
        }
        memcpy(packet.rule_name, event->rule_name, sizeof(packet.rule_name));
        packet.rule_name[sizeof(packet.rule_name) - 1u] = '\0';
    }
    return packet;
}

static int gatt_rule_event_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }
    ble_rule_event_packet_t packet = make_rule_event_packet(NULL);
    int rc = os_mbuf_append(ctxt->om, &packet, sizeof(packet));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_status_char_uuid.u,
                .access_cb = gatt_status_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_status_char_handle,
            },
            {
                .uuid = &s_rule_event_char_uuid.u,
                .access_cb = gatt_rule_event_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_rule_event_char_handle,
            },
            {0},
        },
    },
    {0},
};

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    const char *name = ble_svc_gap_device_name();
    fields.name = (const uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;
    fields.uuids16 = &s_service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "set advertising data failed: rc=%d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "start advertising failed: rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started as %s", CONFIG_APP_BLE_GATT_DEVICE_NAME);
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_connected = true;
            s_status_notify_enabled = false;
            s_rule_event_notify_enabled = false;
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "BLE client connected: handle=%u", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "BLE connection failed: status=%d", event->connect.status);
            advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE client disconnected: reason=%d", event->disconnect.reason);
        s_connected = false;
        s_status_notify_enabled = false;
        s_rule_event_notify_enabled = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_mtu = BLE_GATT_DEFAULT_MTU;
        advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertising complete: reason=%d", event->adv_complete.reason);
        advertise();
        return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_status_char_handle) {
            s_status_notify_enabled = event->subscribe.cur_notify;
            ESP_LOGI(TAG, "status notifications %s", s_status_notify_enabled ? "enabled" : "disabled");
        } else if (event->subscribe.attr_handle == s_rule_event_char_handle) {
            s_rule_event_notify_enabled = event->subscribe.cur_notify;
            ESP_LOGI(TAG, "rule event notifications %s", s_rule_event_notify_enabled ? "enabled" : "disabled");
        }
        return 0;
    case BLE_GAP_EVENT_MTU:
        s_mtu = event->mtu.value;
        ESP_LOGI(TAG, "BLE MTU updated: %u", s_mtu);
        return 0;
    default:
        return 0;
    }
}

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE reset: reason=%d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure BLE address failed: rc=%d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer BLE address failed: rc=%d", rc);
        return;
    }
    advertise();
}

static void ble_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t transport_ble_gatt_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE init failed: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set(CONFIG_APP_BLE_GATT_DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "set BLE device name failed: rc=%d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "count GATT config failed: rc=%d", rc);
        return ESP_FAIL;
    }
    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "add GATT service failed: rc=%d", rc);
        return ESP_FAIL;
    }

    nimble_port_freertos_init(ble_host_task);

    s_started = true;
    ESP_LOGI(TAG, "BLE GATT rule-event transport starting");
    return ESP_OK;
}

bool transport_ble_gatt_is_connected(void)
{
    return s_connected;
}

bool transport_ble_status_notify_enabled(void)
{
    return s_status_notify_enabled;
}

bool transport_ble_rule_event_notify_enabled(void)
{
    return s_rule_event_notify_enabled;
}

esp_err_t transport_ble_send_rule_event(const rule_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_connected || !s_rule_event_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }
    ble_rule_event_packet_t packet = make_rule_event_packet(event);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&packet, sizeof(packet));
    if (om == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_rule_event_char_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

void transport_ble_gatt_update_status(const transport_ble_status_snapshot_t *status)
{
    if (status != NULL) {
        s_status_snapshot = *status;
    }
    if (!s_connected || !s_status_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    ble_rule_status_packet_t packet = make_status_packet();
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&packet, sizeof(packet));
    if (om == NULL) {
        return;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_status_char_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "status notify failed: rc=%d", rc);
    }
}
