#include "transport_ble_gatt_pcm.h"

#include "board_audio_clock.h"
#include "board_sticks3.h"
#include "driver/i2s.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_check.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>

#define BLE_GATT_PCM_APP_ID 0x53
#define BLE_GATT_PCM_SERVICE_INST_ID 0
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

static uint16_t s_adv_service_uuid = BLE_GATT_PCM_SERVICE_UUID;
static const uint16_t s_cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t s_char_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static uint8_t s_initial_char_value[] = {0};
static const uint8_t s_initial_cccd_value[2] = {0x00, 0x00};

static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id;
static uint16_t s_service_handle;
static uint16_t s_char_handle;
static uint16_t s_cccd_handle;
static uint16_t s_mtu = BLE_GATT_PCM_DEFAULT_MTU;
static bool s_connected;
static bool s_notify_enabled;
static bool s_started;

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

static esp_attr_value_t s_char_value = {
    .attr_max_len = BLE_GATT_PCM_MAX_NOTIFY_BYTES,
    .attr_len = sizeof(s_initial_char_value),
    .attr_value = s_initial_char_value,
};

static esp_attr_value_t s_cccd_value = {
    .attr_max_len = sizeof(s_initial_cccd_value),
    .attr_len = sizeof(s_initial_cccd_value),
    .attr_value = (uint8_t *)s_initial_cccd_value,
};

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void start_advertising(void)
{
    esp_err_t err = esp_ble_gap_start_advertising(&s_adv_params);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start advertising failed: %s", esp_err_to_name(err));
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        start_advertising();
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "BLE advertising started as %s", CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME);
        } else {
            ESP_LOGE(TAG, "BLE advertising start failed: status=%d", param->adv_start_cmpl.status);
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "BLE conn params: status=%d interval=%d latency=%d timeout=%d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

static void handle_cccd_write(const esp_ble_gatts_cb_param_t *param)
{
    if (param->write.len != sizeof(uint16_t)) {
        ESP_LOGW(TAG, "unexpected CCCD write length: %u", param->write.len);
        return;
    }

    uint16_t cccd = (uint16_t)param->write.value[0] | ((uint16_t)param->write.value[1] << 8);
    s_notify_enabled = (cccd & 0x0001U) != 0;
    ESP_LOGI(TAG, "PCM notifications %s", s_notify_enabled ? "enabled" : "disabled");
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT: {
        if (param->reg.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "GATTS app register failed: status=%d", param->reg.status);
            break;
        }

        s_gatts_if = gatts_if;
        ESP_ERROR_CHECK(esp_ble_gap_set_device_name(CONFIG_APP_BLE_GATT_PCM_DEVICE_NAME));

        esp_ble_adv_data_t adv_data = {
            .set_scan_rsp = false,
            .include_name = true,
            .include_txpower = true,
            .min_interval = 0x10,
            .max_interval = 0x20,
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data = NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = sizeof(s_adv_service_uuid),
            .p_service_uuid = (uint8_t *)&s_adv_service_uuid,
            .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
        };
        ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));

        esp_gatt_srvc_id_t service_id = {
            .is_primary = true,
            .id.inst_id = BLE_GATT_PCM_SERVICE_INST_ID,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = BLE_GATT_PCM_SERVICE_UUID,
        };
        ESP_ERROR_CHECK(esp_ble_gatts_create_service(gatts_if, &service_id, 6));
        break;
    }
    case ESP_GATTS_CREATE_EVT: {
        if (param->create.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "create service failed: status=%d", param->create.status);
            break;
        }

        s_service_handle = param->create.service_handle;
        esp_bt_uuid_t char_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = BLE_GATT_PCM_CHAR_UUID,
        };
        ESP_ERROR_CHECK(esp_ble_gatts_start_service(s_service_handle));
        ESP_ERROR_CHECK(esp_ble_gatts_add_char(s_service_handle, &char_uuid,
                                               ESP_GATT_PERM_READ,
                                               s_char_property,
                                               &s_char_value,
                                               NULL));
        break;
    }
    case ESP_GATTS_ADD_CHAR_EVT: {
        if (param->add_char.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "add PCM characteristic failed: status=%d", param->add_char.status);
            break;
        }

        s_char_handle = param->add_char.attr_handle;
        esp_bt_uuid_t descr_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = s_cccd_uuid,
        };
        ESP_ERROR_CHECK(esp_ble_gatts_add_char_descr(s_service_handle, &descr_uuid,
                                                     ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                     &s_cccd_value,
                                                     NULL));
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        if (param->add_char_descr.status == ESP_GATT_OK) {
            s_cccd_handle = param->add_char_descr.attr_handle;
            ESP_LOGI(TAG, "BLE GATT PCM service ready: service=0x%04x char=0x%04x",
                     BLE_GATT_PCM_SERVICE_UUID, BLE_GATT_PCM_CHAR_UUID);
        } else {
            ESP_LOGE(TAG, "add PCM CCCD failed: status=%d", param->add_char_descr.status);
        }
        break;
    case ESP_GATTS_CONNECT_EVT: {
        s_connected = true;
        s_notify_enabled = false;
        s_conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "BLE client connected: conn_id=%u", s_conn_id);

        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x10;
        conn_params.min_int = 0x08;
        conn_params.timeout = 400;
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "BLE client disconnected: reason=0x%x", param->disconnect.reason);
        s_connected = false;
        s_notify_enabled = false;
        s_mtu = BLE_GATT_PCM_DEFAULT_MTU;
        start_advertising();
        break;
    case ESP_GATTS_MTU_EVT:
        s_mtu = param->mtu.mtu;
        ESP_LOGI(TAG, "BLE MTU updated: %u", s_mtu);
        break;
    case ESP_GATTS_WRITE_EVT:
        if (!param->write.is_prep && param->write.handle == s_cccd_handle) {
            handle_cccd_write(param);
        }
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                        ESP_GATT_OK, NULL);
        }
        break;
    case ESP_GATTS_CONGEST_EVT:
        ESP_LOGI(TAG, "BLE congestion %s", param->congest.congested ? "on" : "off");
        break;
    default:
        break;
    }
}

static void pcm_notify_task(void *arg)
{
    (void)arg;

    const board_audio_clock_profile_t *clock = board_audio_clock_get_profile();
    uint8_t packet[BLE_GATT_PCM_MAX_NOTIFY_BYTES] = {0};
    ble_gatt_pcm_header_t *header = (ble_gatt_pcm_header_t *)packet;
    uint8_t *payload = packet + sizeof(*header);
    uint32_t sequence = 0;

    header->magic = BLE_GATT_PCM_HEADER_MAGIC;
    header->version = BLE_GATT_PCM_HEADER_VERSION;
    header->header_bytes = sizeof(*header);
    header->sample_rate_hz = clock->sample_rate_hz;
    header->channels = 1;
    header->bits_per_sample = 16;

    while (true) {
        if (!s_connected || !s_notify_enabled || s_char_handle == 0 || s_gatts_if == ESP_GATT_IF_NONE) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t notify_capacity = MIN((size_t)(s_mtu > BLE_GATT_PCM_ATT_HEADER_BYTES ?
                                             s_mtu - BLE_GATT_PCM_ATT_HEADER_BYTES : 0),
                                     (size_t)BLE_GATT_PCM_MAX_NOTIFY_BYTES);
        if (notify_capacity <= sizeof(*header)) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        size_t payload_capacity = notify_capacity - sizeof(*header);
        payload_capacity &= ~(size_t)0x01;
        size_t bytes_read = 0;
        esp_err_t err = i2s_read(BOARD_I2S_PORT, payload, payload_capacity, &bytes_read, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "I2S read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (bytes_read == 0) {
            continue;
        }

        header->sequence = sequence++;
        header->payload_bytes = bytes_read;
        esp_err_t notify_err = esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_char_handle,
                                                           sizeof(*header) + bytes_read,
                                                           packet, false);
        if (notify_err != ESP_OK) {
            ESP_LOGW(TAG, "PCM notify failed: %s", esp_err_to_name(notify_err));
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

esp_err_t transport_ble_gatt_pcm_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT), TAG,
                        "release Classic BT memory failed");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&bt_cfg), TAG, "BT controller init failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BLE), TAG, "BLE controller enable failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "Bluedroid init failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "Bluedroid enable failed");
    ESP_RETURN_ON_ERROR(esp_ble_gatts_register_callback(gatts_event_handler), TAG,
                        "GATTS callback register failed");
    ESP_RETURN_ON_ERROR(esp_ble_gap_register_callback(gap_event_handler), TAG,
                        "GAP callback register failed");
    ESP_RETURN_ON_ERROR(esp_ble_gatts_app_register(BLE_GATT_PCM_APP_ID), TAG,
                        "GATTS app register failed");
    ESP_RETURN_ON_ERROR(esp_ble_gatt_set_local_mtu(BLE_GATT_PCM_MAX_MTU), TAG,
                        "set local MTU failed");

    BaseType_t created = xTaskCreate(pcm_notify_task, "ble_gatt_pcm",
                                     BLE_GATT_PCM_TASK_STACK, NULL,
                                     BLE_GATT_PCM_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to start BLE PCM notify task");
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    ESP_LOGI(TAG, "BLE GATT PCM microphone transport starting");
    return ESP_OK;
}
