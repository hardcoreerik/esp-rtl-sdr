idf_component_register(
    SRCS "main.cpp" "ft_tests.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_rtl_sdr nvs_flash esp_timer
)

# Stamp the driver git SHA into the report so a result file is tied to a tree.
execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE FT_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT FT_GIT_SHA)
    set(FT_GIT_SHA "unknown")
endif()
execute_process(
    COMMAND git status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    OUTPUT_VARIABLE FT_GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(FT_GIT_DIRTY)
    set(FT_GIT_SHA "${FT_GIT_SHA}-dirty")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE FT_GIT_SHA="${FT_GIT_SHA}")
