# esp_rtl_sdr — standalone ESP-IDF component (repo root = component root)
#
# Requires ESP-IDF USB Host. High-Speed host is required for the measured
# Blog V4 profile on ESP32-P4. Other hosts/tuners only when measured.

idf_component_register(
    SRCS "src/esp_rtl_sdr.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"
    REQUIRES log esp_timer freertos usb
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
