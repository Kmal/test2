set(APP_SRCS
    "main.c"
    "register_bus.c"
    "board_i2c.c"
    "m5pm1.c"
    "status_ui.c"
    "status_lcd.c"
    "ui_keyboard.c"
    "ui_render.c"
    "display_text.c"
    "app_mode.c"
    "app_time.c"
    "ui_nav.c"
    "ui_model.c"
    "app_wifi.c"
    "app_sound_level_demand.c"
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
    "board_power.c"
    "bmi270.c"
    "board_adc.c"
    "hardware_fact_service.c"
)
set(APP_REQUIRES esp_system nvs_flash esp_netif esp_event esp_lcd bt esp_driver_i2c esp_driver_gpio esp_driver_spi esp_http_client esp_http_server esp_wifi esp_driver_rmt esp_driver_i2s esp_adc)

if(CONFIG_APP_TRANSPORT_BLE_GATT_PCM)
    list(APPEND APP_SRCS "transport_ble_gatt.c")
endif()

if(CONFIG_APP_TRANSPORT_HFP_LEGACY)
    list(APPEND APP_SRCS "transport_hfp_legacy.c")
endif()

if(CONFIG_APP_SOUND_LEVEL_TRIGGERS)
    list(APPEND APP_SRCS
        "audio_metrics.c"
        "board_audio.c"
        "board_audio_clock.c"
        "board_audio_power.c"
        "board_i2s.c"
        "es8311.c"
        "sound_level_service.c"
    )
endif()

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS "."
    REQUIRES ${APP_REQUIRES}
)
