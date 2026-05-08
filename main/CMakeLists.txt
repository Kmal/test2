idf_component_register(
    SRCS "main.c" "es8311.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt driver esp_system nvs_flash
)
    PRIV_REQUIRES bt driver nvs_flash
)
    SRCS "main.c" "es8311.c" "status_ui.c"
    INCLUDE_DIRS "."
)
idf_component_register(SRCS "main.c" "es8311.c" "audio_resample.c"
                       INCLUDE_DIRS ".")
