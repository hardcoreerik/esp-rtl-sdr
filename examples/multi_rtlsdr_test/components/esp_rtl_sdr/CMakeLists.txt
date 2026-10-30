get_filename_component(ESP_RTL_SDR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

idf_component_register(
    SRCS
        "${ESP_RTL_SDR_ROOT}/src/esp_rtl_sdr.cpp"
        "${ESP_RTL_SDR_ROOT}/src/esp_rtl_sdr_policy.cpp"
        "${ESP_RTL_SDR_ROOT}/src/esp_rtl_sdr_multi.cpp"
    INCLUDE_DIRS
        "${ESP_RTL_SDR_ROOT}/include"
    PRIV_INCLUDE_DIRS
        "${ESP_RTL_SDR_ROOT}/private"
    REQUIRES log esp_timer freertos usb
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)

# IDF 6.x defaults to a newer dialect; this component targets C++17.
target_compile_options(${COMPONENT_LIB} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>")
