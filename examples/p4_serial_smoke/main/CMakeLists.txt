set(smoke_board_requires)
if(CONFIG_ESP_RTL_SDR_SMOKE_BOARD_TAB5)
    list(APPEND smoke_board_requires m5unified)
endif()
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_rtl_sdr nvs_flash ${smoke_board_requires}
)
