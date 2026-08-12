# Standalone host unit tests (no ESP-IDF).
cmake_minimum_required(VERSION 3.16)
project(esp_rtl_sdr_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(esp_rtl_sdr_host_tests
    test_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
)

target_include_directories(esp_rtl_sdr_host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

enable_testing()
add_test(NAME policy COMMAND esp_rtl_sdr_host_tests)
