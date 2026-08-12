cmake_minimum_required(VERSION 3.16)

# Repo root is the component root for this stand-alone project.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_rtl_sdr_p4_smoke)
