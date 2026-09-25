cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "0.8.0-rc3")

if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_rtl_sdr_multi_test)
