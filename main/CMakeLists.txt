idf_component_register(
    SRCS "main.c" "es8311.c" "status_ui.c" "audio_resample.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt driver esp_system nvs_flash
)
