idf_component_register(
    SRCS "main.c" "es8311.c" "status_ui.c"
    INCLUDE_DIRS "."
)
idf_component_register(SRCS "main.c" "es8311.c" "audio_resample.c"
                       INCLUDE_DIRS ".")
