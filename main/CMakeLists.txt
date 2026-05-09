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
    "audio_resample.c"
    "audio_pipeline.c"
    "button_state.c"
)
set(APP_PRIV_REQUIRES driver esp_system nvs_flash)

if(CONFIG_APP_TRANSPORT_BLE_GATT_PCM)
    list(APPEND APP_SRCS "transport_ble_gatt_pcm.c")
    list(APPEND APP_PRIV_REQUIRES bt)
endif()

if(CONFIG_APP_TRANSPORT_HFP_LEGACY)
    list(APPEND APP_SRCS "transport_hfp_legacy.c")
    list(APPEND APP_PRIV_REQUIRES bt)
endif()

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${APP_PRIV_REQUIRES}
)
