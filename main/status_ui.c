#include "status_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef STATUS_UI_BUTTON_CLEAR_PAIRING_GPIO
#define STATUS_UI_BUTTON_CLEAR_PAIRING_GPIO 37
#endif

#ifndef STATUS_UI_BUTTON_TOGGLE_MONITORING_GPIO
#define STATUS_UI_BUTTON_TOGGLE_MONITORING_GPIO 39
#endif

#ifndef STATUS_UI_BUTTON_TOGGLE_DISCOVERABLE_GPIO
#define STATUS_UI_BUTTON_TOGGLE_DISCOVERABLE_GPIO 35
#endif

#define STATUS_UI_BUTTON_ACTIVE_LEVEL 0
#define STATUS_UI_DEBOUNCE_MS 50
#define STATUS_UI_POLL_MS 25
#define STATUS_UI_TASK_STACK 2048
#define STATUS_UI_TASK_PRIORITY 5

static const char *TAG = "STATUS_UI";

static status_ui_button_handlers_t s_handlers;
static status_ui_state_t s_state = STATUS_UI_STATE_BOOTING;
static bool s_monitoring_enabled = true;
static bool s_discoverable_enabled = false;

typedef struct {
    gpio_num_t gpio;
    const char *name;
    void (*handler)(void *ctx);
    bool stable_pressed;
    bool last_sample_pressed;
    TickType_t last_change_tick;
} status_button_t;

static const char *bool_label(bool enabled)
{
    return enabled ? "on" : "off";
}

const char *status_ui_state_name(status_ui_state_t state)
{
    switch (state) {
    case STATUS_UI_STATE_BOOTING:
        return "booting";
    case STATUS_UI_STATE_DISCOVERABLE:
        return "discoverable";
    case STATUS_UI_STATE_PAIRED:
        return "paired";
    case STATUS_UI_STATE_HFP_CONNECTED:
        return "HFP connected";
    case STATUS_UI_STATE_AUDIO_STREAMING:
        return "audio streaming";
    case STATUS_UI_STATE_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void status_ui_set_state(status_ui_state_t state)
{
    if (s_state == state) {
        return;
    }

    s_state = state;
    ESP_LOGI(TAG, "status: %s", status_ui_state_name(state));
}

status_ui_state_t status_ui_get_state(void)
{
    return s_state;
}

void status_ui_set_monitoring_enabled(bool enabled)
{
    s_monitoring_enabled = enabled;
    ESP_LOGI(TAG, "monitoring output: %s", bool_label(enabled));
}

bool status_ui_get_monitoring_enabled(void)
{
    return s_monitoring_enabled;
}

void status_ui_set_discoverable_enabled(bool enabled)
{
    s_discoverable_enabled = enabled;
    ESP_LOGI(TAG, "discoverable mode: %s", bool_label(enabled));
}

bool status_ui_get_discoverable_enabled(void)
{
    return s_discoverable_enabled;
}

static bool button_is_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == STATUS_UI_BUTTON_ACTIVE_LEVEL;
}

static void maybe_dispatch_button(status_button_t *button, TickType_t now)
{
    bool pressed = button_is_pressed(button->gpio);

    if (pressed != button->last_sample_pressed) {
        button->last_sample_pressed = pressed;
        button->last_change_tick = now;
        return;
    }

    if ((now - button->last_change_tick) < pdMS_TO_TICKS(STATUS_UI_DEBOUNCE_MS)) {
        return;
    }

    if (pressed == button->stable_pressed) {
        return;
    }

    button->stable_pressed = pressed;
    if (pressed && button->handler != NULL) {
        ESP_LOGI(TAG, "button pressed: %s", button->name);
        button->handler(s_handlers.ctx);
    }
}

static void status_ui_button_task(void *arg)
{
    (void)arg;

    status_button_t buttons[] = {
        {
            .gpio = STATUS_UI_BUTTON_CLEAR_PAIRING_GPIO,
            .name = "clear pairing",
            .handler = s_handlers.clear_pairing,
        },
        {
            .gpio = STATUS_UI_BUTTON_TOGGLE_MONITORING_GPIO,
            .name = "toggle monitoring output",
            .handler = s_handlers.toggle_monitoring,
        },
        {
            .gpio = STATUS_UI_BUTTON_TOGGLE_DISCOVERABLE_GPIO,
            .name = "toggle discoverable mode",
            .handler = s_handlers.toggle_discoverable,
        },
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        buttons[i].stable_pressed = button_is_pressed(buttons[i].gpio);
        buttons[i].last_sample_pressed = buttons[i].stable_pressed;
        buttons[i].last_change_tick = xTaskGetTickCount();
    }

    while (true) {
        TickType_t now = xTaskGetTickCount();
        for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
            maybe_dispatch_button(&buttons[i], now);
        }
        vTaskDelay(pdMS_TO_TICKS(STATUS_UI_POLL_MS));
    }
}

esp_err_t status_ui_init(const status_ui_button_handlers_t *handlers)
{
    if (handlers != NULL) {
        memcpy(&s_handlers, handlers, sizeof(s_handlers));
    } else {
        memset(&s_handlers, 0, sizeof(s_handlers));
    }

    uint64_t pin_mask = (1ULL << STATUS_UI_BUTTON_CLEAR_PAIRING_GPIO) |
                        (1ULL << STATUS_UI_BUTTON_TOGGLE_MONITORING_GPIO) |
                        (1ULL << STATUS_UI_BUTTON_TOGGLE_DISCOVERABLE_GPIO);
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure buttons: %s", esp_err_to_name(err));
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return err;
    }

    BaseType_t created = xTaskCreate(status_ui_button_task, "status_ui_buttons",
                                     STATUS_UI_TASK_STACK, NULL,
                                     STATUS_UI_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to start button task");
        status_ui_set_state(STATUS_UI_STATE_ERROR);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "status UI ready; buttons clear=%d monitor=%d discoverable=%d",
             STATUS_UI_BUTTON_CLEAR_PAIRING_GPIO,
             STATUS_UI_BUTTON_TOGGLE_MONITORING_GPIO,
             STATUS_UI_BUTTON_TOGGLE_DISCOVERABLE_GPIO);
    ESP_LOGI(TAG, "status: %s", status_ui_state_name(s_state));
    ESP_LOGI(TAG, "monitoring output: %s", bool_label(s_monitoring_enabled));
    return ESP_OK;
}
