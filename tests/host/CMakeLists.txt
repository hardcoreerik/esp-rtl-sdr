# Standalone host unit tests (no ESP-IDF).
cmake_minimum_required(VERSION 3.16)
project(esp_rtl_sdr_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(esp_rtl_sdr_host_tests
    test_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
)

add_executable(esp_rtl_sdr_profile_tests
    test_profiles.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
)

add_executable(esp_rtl_sdr_multi_tests
    test_multi_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/esp_rtl_sdr_multi.cpp
)

add_executable(esp_rtl_sdr_band_tests
    test_r820t2_band.cpp
)

target_include_directories(esp_rtl_sdr_band_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

add_executable(esp_rtl_sdr_v4_band_tests
    test_v4_band.cpp
)

target_include_directories(esp_rtl_sdr_v4_band_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

add_executable(esp_rtl_sdr_usb_guard_tests
    test_usb_guard.cpp
)

target_include_directories(esp_rtl_sdr_usb_guard_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

target_include_directories(esp_rtl_sdr_host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
    ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/p4_serial_smoke/main
)

target_include_directories(esp_rtl_sdr_profile_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

target_include_directories(esp_rtl_sdr_multi_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../private
)

if(MSVC)
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_multi_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_usb_guard_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_v4_band_tests PRIVATE /W4)
  target_compile_options(esp_rtl_sdr_band_tests PRIVATE /W4)
else()
  target_compile_options(esp_rtl_sdr_host_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_profile_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_multi_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_usb_guard_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_v4_band_tests PRIVATE -Wall -Wextra)
  target_compile_options(esp_rtl_sdr_band_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME policy COMMAND esp_rtl_sdr_host_tests)
add_test(NAME profiles COMMAND esp_rtl_sdr_profile_tests)
add_test(NAME multi_device COMMAND esp_rtl_sdr_multi_tests)
add_test(NAME usb_guard COMMAND esp_rtl_sdr_usb_guard_tests)
add_test(NAME v4_band COMMAND esp_rtl_sdr_v4_band_tests)
add_test(NAME r820t2_band COMMAND esp_rtl_sdr_band_tests)

add_executable(esp_rtl_sdr_usb_shutdown_tests test_usb_shutdown.cpp)
target_include_directories(esp_rtl_sdr_usb_shutdown_tests PRIVATE stubs ../../private)
add_test(NAME usb_shutdown COMMAND esp_rtl_sdr_usb_shutdown_tests)

add_executable(esp_rtl_sdr_pull_alloc_tests test_pull_alloc.cpp)
target_include_directories(esp_rtl_sdr_pull_alloc_tests PRIVATE ../../private)
add_test(NAME pull_alloc COMMAND esp_rtl_sdr_pull_alloc_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../src/esp_rtl_sdr.cpp ../scripts/generate_lifecycle_test.py)
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/generate_lifecycle_test.py
            ${CMAKE_CURRENT_BINARY_DIR}/test_actual_session.cpp
    RESULT_VARIABLE lifecycle_generator_result
)
if(NOT lifecycle_generator_result EQUAL 0)
    message(FATAL_ERROR "Could not generate lifecycle host tests")
endif()
add_executable(esp_rtl_sdr_actual_session_tests ${CMAKE_CURRENT_BINARY_DIR}/test_actual_session.cpp)
target_include_directories(esp_rtl_sdr_actual_session_tests PRIVATE stubs ../../private)
add_test(NAME actual_session COMMAND esp_rtl_sdr_actual_session_tests)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ../../examples/p4_serial_smoke/main/main.cpp)
foreach(board TAB5 GENERIC UNKNOWN)
    add_executable(esp_rtl_sdr_smoke_${board}_tests ${CMAKE_CURRENT_BINARY_DIR}/test_actual_smoke.cpp)
    target_compile_definitions(esp_rtl_sdr_smoke_${board}_tests PRIVATE CONFIG_ESP_RTL_SDR_SMOKE_BOARD_${board}=1)
    add_test(NAME smoke_${board} COMMAND esp_rtl_sdr_smoke_${board}_tests)
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS uninstall_harness.cpp.in)
add_executable(esp_rtl_sdr_actual_uninstall_tests ${CMAKE_CURRENT_BINARY_DIR}/test_actual_uninstall.cpp)
target_include_directories(esp_rtl_sdr_actual_uninstall_tests PRIVATE stubs)
add_test(NAME actual_uninstall COMMAND esp_rtl_sdr_actual_uninstall_tests)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS start_alloc_harness.cpp.in)
add_executable(esp_rtl_sdr_actual_start_alloc_tests ${CMAKE_CURRENT_BINARY_DIR}/test_actual_start_alloc.cpp ../../src/esp_rtl_sdr_policy.cpp)
target_include_directories(esp_rtl_sdr_actual_start_alloc_tests PRIVATE stubs ../../include ../../private)
add_test(NAME actual_start_alloc COMMAND esp_rtl_sdr_actual_start_alloc_tests)
