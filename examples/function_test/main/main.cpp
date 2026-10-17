/*
 * esp_rtl_sdr function-test firmware.
 *
 * Runs the whole suite once on boot and prints one JSON line per result (see
 * ft_core.hpp), then a summary. Press Enter on the console to run it again.
 * tests/scripts/function_test_runner.py flashes/resets, captures the log and
 * turns the summary into an exit code.
 */
#include <cstdio>

#include "esp_log.h"
#include "esp_rtl_sdr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ft_core.hpp"
#include "ft_tests.hpp"
#include "nvs_flash.h"
#include "sdkconfig.h"

#ifndef FT_GIT_SHA
#define FT_GIT_SHA "unknown"
#endif

static void sink(const char *line, void *)
{
    std::printf("%s\n", line);
    std::fflush(stdout);
}

extern "C" void app_main(void)
{
    (void)nvs_flash_init();
    /* The driver logs heavily; keep the console to the report. */
    esp_log_level_set("*", ESP_LOG_WARN);

    while (true) {
        std::printf("FT_BEGIN\n");
        ft::Reporter r(sink, nullptr);
        r.begin(esp_rtl_sdr_get_version_string(), FT_GIT_SHA, CONFIG_IDF_TARGET);
        ft_run_all(r);
        r.summary(ft_devices_present());
        std::printf("FT_END rc=%d\n", r.ok() ? 0 : 1);
        std::printf("FT_IDLE press Enter to run again\n");
        std::fflush(stdout);

        char line[16];
        while (std::fgets(line, sizeof(line), stdin) == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}
