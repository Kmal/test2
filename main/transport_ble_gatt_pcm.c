#include "transport_ble_gatt_pcm.h"

#include "board_audio_clock.h"
#include "board_sticks3.h"
#include "board_i2s.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "sdkconfig.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#define BLE_GATT_PCM_SERVICE_UUID 0xFFF0
#define BLE_GATT_PCM_CHAR_UUID 0xFFF1
#define BLE_GATT_PCM_TASK_STACK 4096
#define BLE_GATT_PCM_TASK_PRIORITY 5
#define BLE_GATT_PCM_HEADER_MAGIC 0x33534d35U /* "M5S3" little-endian */
#define BLE_GATT_PCM_HEADER_VERSION 1U
#define BLE_GATT_PCM_DEFAULT_MTU 23U
#define BLE_GATT_PCM_ATT_HEADER_BYTES 3U
#define BLE_GATT_PCM_MAX_MTU 247U
#define BLE_GATT_PCM_MAX_NOTIFY_BYTES (BLE_GATT_PCM_MAX_MTU - BLE_GATT_PCM_ATT_HEADER_BYTES)

static const char *TAG = "BLE_GATT_PCM";

static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_char_handle;
static uint16_t s_mtu = BLE_GATT_PCM_DEFAULT_MTU;
static bool s_connected;
static bool s_notify_enabled;
static bool s_started;

static const ble_uuid16_t s_service_uuid = BLE_UUID16_INIT(BLE_GATT_PCM_SERVICE_UUID);
static const ble_uuid16_t s_char_uuid = BLE_UUID16_INIT(BLE_GATT_PCM_CHAR_UUID);

/* Last-readable value; notifications carry framed PCM payloads. */
static uint8_t s_initial_char_value[] = {0};

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_bytes;
    uint32_t sequence;
    uint32_t sample_rate_hz;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint16_t payload_bytes;
    uint16_t reserved;
} ble_gatt_pcm_header_t;

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    int rc = os_mbuf_append(ctxt->om, s_initial_char_value, sizeof(s_initial_char_value));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gap_event_cb(struct ble_gap_event *event, void *arg);

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_char_uuid.u,
                .access_cb = gatt_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_char_handle,
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
        ESP_LOGI(TAG, "BLE advertising started as %s", CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME);
    }
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_connected = true;
            s_notify_enabled = false;
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
        s_notify_enabled = false;
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_mtu = BLE_GATT_PCM_DEFAULT_MTU;
        advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "BLE advertising complete: reason=%d", event->adv_complete.reason);
        advertise();
        return 0;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == s_char_handle) {
            s_notify_enabled = event->subscribe.cur_notify;
            ESP_LOGI(TAG, "PCM notifications %s", s_notify_enabled ? "enabled" : "disabled");
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
        ESP_LOGE(TAG, "infer BLE address type failed: rc=%d", rc);
        return;
    }

    advertise();
}

static void ble_host_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void pcm_notify_task(void *arg)
{
    (void)arg;
    const board_audio_clock_profile_t *clock = board_audio_clock_get_profile();
    uint32_t sequence = 0;
    uint8_t notify_buf[BLE_GATT_PCM_MAX_NOTIFY_BYTES];
    const size_t header_bytes = sizeof(ble_gatt_pcm_header_t);

    while (true) {
        if (!s_connected || !s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        const uint16_t att_payload_limit = (s_mtu > BLE_GATT_PCM_ATT_HEADER_BYTES)
                                             ? (uint16_t)(s_mtu - BLE_GATT_PCM_ATT_HEADER_BYTES)
                                             : 0;
        const size_t notify_capacity = MIN((size_t)att_payload_limit, sizeof(notify_buf));
        if (notify_capacity <= header_bytes) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        uint8_t *payload = notify_buf + header_bytes;
        const size_t payload_capacity = notify_capacity - header_bytes;
        size_t bytes_read = 0;
        esp_err_t err = board_i2s_read(payload, payload_capacity, &bytes_read, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (bytes_read == 0) {
            continue;
        }

        ble_gatt_pcm_header_t header = {
            .magic = BLE_GATT_PCM_HEADER_MAGIC,
            .version = BLE_GATT_PCM_HEADER_VERSION,
            .header_bytes = header_bytes,
            .sequence = sequence++,
            .sample_rate_hz = (uint32_t)clock->sample_rate_hz,
            .channels = (uint16_t)clock->channels,
            .bits_per_sample = (uint16_t)clock->bits_per_sample,
            .payload_bytes = (uint16_t)bytes_read,
            .reserved = 0,
        };
        memcpy(notify_buf, &header, sizeof(header));

        struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_buf, header_bytes + bytes_read);
        if (om == NULL) {
            ESP_LOGW(TAG, "failed to allocate BLE notification buffer");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int rc = ble_gatts_notify_custom(s_conn_handle, s_char_handle, om);
        if (rc != 0) {
            ESP_LOGW(TAG, "BLE notify failed: rc=%d", rc);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

esp_err_t transport_ble_gatt_pcm_start(void)
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

    int rc = ble_svc_gap_device_name_set(CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME);
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

    BaseType_t task_ok = xTaskCreate(pcm_notify_task, "ble_pcm_notify", BLE_GATT_PCM_TASK_STACK,
                                     NULL, BLE_GATT_PCM_TASK_PRIORITY, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create BLE PCM notify task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    ESP_LOGI(TAG, "BLE GATT PCM transport starting");
    return ESP_OK;
}
