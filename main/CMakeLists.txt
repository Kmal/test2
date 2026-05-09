set(APP_SRCS "main.c" "es8311.c" "status_ui.c" "audio_resample.c" "audio_pipeline.c" "button_state.c")
set(APP_PRIV_REQUIRES driver esp_system nvs_flash)

if(CONFIG_APP_TRANSPORT_HFP_LEGACY)
    list(APPEND APP_SRCS "transport_hfp_legacy.c")
    list(APPEND APP_PRIV_REQUIRES bt)
endif()

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${APP_PRIV_REQUIRES}
)
