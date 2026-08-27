cmake_minimum_required(VERSION 3.16)

# Pin app version so idf.py project_description is 0.7.12, not git-describe
# of an older tag.
set(PROJECT_VER "0.7.12")

# Component is examples/p4_serial_smoke/components/esp_rtl_sdr (stable name).
# Do not set EXTRA_COMPONENT_DIRS to the repo root - the component name would
# become the checkout folder name (e.g. esp-rtl-sdr).

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_rtl_sdr_p4_smoke)