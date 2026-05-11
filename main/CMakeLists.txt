set(APP_SRCS
    "main.c"
    "register_bus.c"
    "board_i2c.c"
    "board_i2s.c"
    "board_audio_clock.c"
    "board_audio.c"
    "board_audio_power.c"
    "m5pm1.c"
    "es8311.c"
    "status_ui.c"
    "app_mode.c"
    "ui_nav.c"
    "app_wifi.c"
    "audio_metrics.c"
    "sound_meter.c"
    "pcm_debug_ring.c"
    "audio_resample.c"
    "audio_pipeline.c"
    "button_state.c"
    "rule_types.c"
    "rule_engine.c"
    "capability_registry.c"
    "trigger_sources.c"
    "action_dispatcher.c"
    "rule_runtime.c"
    "trigger_gpio.c"
    "trigger_hat.c"
    "action_http.c"
    "action_ir.c"
    "action_hat.c"
    "rule_config_store.c"
    "rule_web.c"
)
set(APP_REQUIRES esp_system nvs_flash esp_netif esp_event esp_lcd bt esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_driver_spi esp_http_client esp_http_server esp_wifi esp_driver_rmt)

if(CONFIG_APP_TRANSPORT_BLE_GATT_PCM)
    list(APPEND APP_SRCS "transport_ble_gatt_pcm.c")
endif()

if(CONFIG_APP_TRANSPORT_HFP_LEGACY)
    list(APPEND APP_SRCS "transport_hfp_legacy.c")
endif()

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS "."
    REQUIRES ${APP_REQUIRES}
)
