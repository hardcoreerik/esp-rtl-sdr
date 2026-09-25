/*
 * Multi-receiver hardware validation harness for esp_rtl_sdr.
 *
 * Serial CLI (UART). Up to three independent handles share one USB host
 * session. Not RF-coherent: host-timestamped concurrent capture only.
 *
 * Commands: type "help".
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_log.h"
#include "esp_rtl_sdr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "multi_rtl";

static constexpr int kMaxDev = 3;
static constexpr uint32_t kDefaultSps = ESP_RTL_SDR_RATE_2400K;

static esp_rtl_sdr_handle_t s_dev[kMaxDev];
static volatile uint32_t s_iq_blocks[kMaxDev];
static volatile int s_watch_ms;

static void on_event(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(ctx));
    if (ev == ESP_RTL_SDR_EVT_IQ_BLOCK) {
        if (idx >= 0 && idx < kMaxDev) {
            s_iq_blocks[idx]++;
        }
        return;
    }
    if (ev == ESP_RTL_SDR_EVT_DISCONNECTED) {
        ESP_LOGW(TAG, "[RTL%d] disconnected (others keep running)", idx);
    } else if (ev == ESP_RTL_SDR_EVT_READY) {
        ESP_LOGI(TAG, "[RTL%d] ready", idx);
    } else if (ev == ESP_RTL_SDR_EVT_STREAM_STARTED) {
        ESP_LOGI(TAG, "[RTL%d] stream started", idx);
    } else if (ev == ESP_RTL_SDR_EVT_STOPPED) {
        ESP_LOGI(TAG, "[RTL%d] stream stopped", idx);
    } else if (ev == ESP_RTL_SDR_EVT_ERROR && payload != nullptr) {
        const auto *err = static_cast<const esp_rtl_sdr_error_info_t *>(payload);
        ESP_LOGE(TAG, "[RTL%d] error %s %s", idx, esp_rtl_sdr_err_to_name(err->code),
                 err->message);
    }
    (void)payload;
}

static bool valid_idx(int idx)
{
    return idx >= 0 && idx < kMaxDev && s_dev[idx] != nullptr;
}

static void print_help(void)
{
    printf("multi_rtlsdr_test  commands:\n"
           "  help\n"
           "  rtl list                 list enumerated RTL-SDR candidates\n"
           "  rtl info N               identity + tuner of handle N (0..2)\n"
           "  rtl freq N HZ            set center frequency (Hz)\n"
           "  rtl rate N SPS           set sample rate (idle only)\n"
           "  rtl gain N TENTH_DB      set manual gain (tenths dB, e.g. 280)\n"
           "  rtl gainmode N auto|manual\n"
           "  rtl stream N start|stop  start/stop one receiver\n"
           "  rtl stream all start|stop\n"
           "  rtl stats                per-device + hub counters\n"
           "  rtl hub                  USB session / hub stats\n"
           "  rtl watch MS             print stats every MS (0=off)\n"
           "  rtl stopall\n"
           "Keep bias tees OFF during multi-dongle bring-up.\n"
           "Captures are concurrent and host-timestamped, not phase-coherent.\n");
}

static void cmd_list(void)
{
    if (s_dev[0] == nullptr) {
        printf("no handle\n");
        return;
    }
    (void)esp_rtl_sdr_refresh_device_list(s_dev[0]);
    size_t n = 0;
    (void)esp_rtl_sdr_get_device_count(s_dev[0], &n);
    printf("candidates=%u\n", (unsigned)n);
    for (size_t i = 0; i < n; ++i) {
        esp_rtl_sdr_identity_t id;
        if (esp_rtl_sdr_get_candidate_identity(s_dev[0], i, &id) != ESP_OK) {
            continue;
        }
        printf("  [%u] addr=%u path=%s serial=%s mfg=%s prod=%s tuner=%s hs=%d\n",
               (unsigned)i, (unsigned)id.usb_addr, id.usb_path, id.serial, id.manufacturer,
               id.product, esp_rtl_sdr_profile_to_name(id.profile), (int)id.high_speed);
    }
}

static void cmd_info(int idx)
{
    if (!valid_idx(idx)) {
        printf("bad index %d\n", idx);
        return;
    }
    esp_rtl_sdr_identity_t id;
    esp_rtl_sdr_capture_meta_t meta;
    uint32_t freq = 0, sps = 0;
    int gain = 0;
    (void)esp_rtl_sdr_get_identity(s_dev[idx], &id);
    (void)esp_rtl_sdr_get_capture_meta(s_dev[idx], &meta);
    (void)esp_rtl_sdr_get_center_freq(s_dev[idx], &freq);
    (void)esp_rtl_sdr_get_sample_rate(s_dev[idx], &sps);
    (void)esp_rtl_sdr_get_tuner_gain(s_dev[idx], &gain);
    printf("[RTL%d] state=%s present=%d addr=%u path=%s serial=%s tuner=%s\n", idx,
           esp_rtl_sdr_state_to_name(esp_rtl_sdr_get_state(s_dev[idx])), (int)id.present,
           (unsigned)id.usb_addr, id.usb_path, id.serial,
           esp_rtl_sdr_profile_to_name(id.profile));
    printf("[RTL%d] freq=%u tuner_lo=%u sps=%u gain_tenth=%d seq=%u ts_us=%lld\n", idx,
           (unsigned)freq, (unsigned)meta.tuner_frequency_hz, (unsigned)sps, gain,
           (unsigned)meta.sequence, (long long)meta.host_timestamp_us);
}

static void print_stats_one(int idx)
{
    if (!valid_idx(idx)) {
        return;
    }
    esp_rtl_sdr_stream_stats_t st;
    if (esp_rtl_sdr_get_stream_stats(s_dev[idx], &st) != ESP_OK) {
        return;
    }
    printf("[RTL%d] bytes=%llu samples=%llu xfer=%u err=%u to=%u short=%u over=%u "
           "drop=%u qhw=%u sps=%u up_ms=%u last_ts=%lld iq_cb=%u\n",
           idx, (unsigned long long)st.bytes_received, (unsigned long long)st.samples_received,
           (unsigned)st.usb_transfer_count, (unsigned)st.usb_transfer_errors,
           (unsigned)st.usb_timeouts, (unsigned)st.short_transfers, (unsigned)st.buffer_overruns,
           (unsigned)st.dropped_buffers, (unsigned)st.queue_high_water,
           (unsigned)st.effective_sample_rate, (unsigned)st.stream_uptime_ms,
           (long long)st.last_transfer_timestamp_us, (unsigned)s_iq_blocks[idx]);
}

static void cmd_stats(void)
{
    for (int i = 0; i < kMaxDev; ++i) {
        print_stats_one(i);
    }
    esp_rtl_sdr_hub_stats_t hub;
    (void)esp_rtl_sdr_get_hub_stats(s_dev[0], &hub);
    printf("hub ref=%u claimed=%u new=%u gone=%u conflicts=%u host=%d hubs_kconfig=%d\n",
           (unsigned)hub.session_refcount, (unsigned)hub.claimed_rtl_count,
           (unsigned)hub.new_dev_events, (unsigned)hub.gone_events,
           (unsigned)hub.claim_conflicts, (int)hub.host_installed, (int)hub.hubs_compiled_in);
}

static esp_err_t start_one(int idx, uint32_t freq, uint32_t sps)
{
    if (!valid_idx(idx)) {
        return ESP_RTL_SDR_ERR_BAD_DEVICE;
    }
    (void)esp_rtl_sdr_set_center_freq(s_dev[idx], freq);
    (void)esp_rtl_sdr_set_sample_rate(s_dev[idx], sps);
    const esp_err_t err = esp_rtl_sdr_start_hz(s_dev[idx], freq, sps);
    printf("[RTL%d] stream start freq=%u sps=%u -> %s\n", idx, (unsigned)freq, (unsigned)sps,
           esp_rtl_sdr_err_to_name(err));
    return err;
}

static void stop_one(int idx)
{
    if (!valid_idx(idx)) {
        return;
    }
    const esp_err_t err = esp_rtl_sdr_stop(s_dev[idx], 0);
    printf("[RTL%d] stream stop -> %s\n", idx, esp_rtl_sdr_err_to_name(err));
}

static void handle_line(char *line)
{
    char *tok = strtok(line, " \t\r\n");
    if (tok == nullptr) {
        return;
    }
    if (strcmp(tok, "help") == 0 || strcmp(tok, "?") == 0) {
        print_help();
        return;
    }
    if (strcmp(tok, "rtl") != 0) {
        printf("unknown. try help\n");
        return;
    }
    char *cmd = strtok(nullptr, " \t\r\n");
    if (cmd == nullptr) {
        print_help();
        return;
    }
    if (strcmp(cmd, "list") == 0) {
        cmd_list();
        return;
    }
    if (strcmp(cmd, "stats") == 0) {
        cmd_stats();
        return;
    }
    if (strcmp(cmd, "hub") == 0) {
        cmd_stats();
        return;
    }
    if (strcmp(cmd, "stopall") == 0) {
        for (int i = 0; i < kMaxDev; ++i) {
            stop_one(i);
        }
        return;
    }
    if (strcmp(cmd, "watch") == 0) {
        char *ms = strtok(nullptr, " \t\r\n");
        s_watch_ms = ms ? atoi(ms) : 0;
        printf("watch=%d ms\n", (int)s_watch_ms);
        return;
    }
    if (strcmp(cmd, "info") == 0) {
        char *n = strtok(nullptr, " \t\r\n");
        cmd_info(n ? atoi(n) : -1);
        return;
    }
    if (strcmp(cmd, "freq") == 0) {
        char *n = strtok(nullptr, " \t\r\n");
        char *hz = strtok(nullptr, " \t\r\n");
        const int idx = n ? atoi(n) : -1;
        if (!valid_idx(idx) || hz == nullptr) {
            printf("usage: rtl freq N HZ\n");
            return;
        }
        const esp_err_t err = esp_rtl_sdr_set_center_freq(s_dev[idx], (uint32_t)strtoul(hz, nullptr, 10));
        printf("[RTL%d] freq -> %s\n", idx, esp_rtl_sdr_err_to_name(err));
        return;
    }
    if (strcmp(cmd, "rate") == 0) {
        char *n = strtok(nullptr, " \t\r\n");
        char *sps = strtok(nullptr, " \t\r\n");
        const int idx = n ? atoi(n) : -1;
        if (!valid_idx(idx) || sps == nullptr) {
            printf("usage: rtl rate N SPS\n");
            return;
        }
        const esp_err_t err =
            esp_rtl_sdr_set_sample_rate(s_dev[idx], (uint32_t)strtoul(sps, nullptr, 10));
        printf("[RTL%d] rate -> %s\n", idx, esp_rtl_sdr_err_to_name(err));
        return;
    }
    if (strcmp(cmd, "gain") == 0) {
        char *n = strtok(nullptr, " \t\r\n");
        char *g = strtok(nullptr, " \t\r\n");
        const int idx = n ? atoi(n) : -1;
        if (!valid_idx(idx) || g == nullptr) {
            printf("usage: rtl gain N TENTH_DB\n");
            return;
        }
        (void)esp_rtl_sdr_set_tuner_gain_mode(s_dev[idx], ESP_RTL_SDR_GAIN_MODE_MANUAL);
        const esp_err_t err = esp_rtl_sdr_set_tuner_gain(s_dev[idx], atoi(g));
        printf("[RTL%d] gain -> %s\n", idx, esp_rtl_sdr_err_to_name(err));
        return;
    }
    if (strcmp(cmd, "gainmode") == 0) {
        char *n = strtok(nullptr, " \t\r\n");
        char *m = strtok(nullptr, " \t\r\n");
        const int idx = n ? atoi(n) : -1;
        if (!valid_idx(idx) || m == nullptr) {
            printf("usage: rtl gainmode N auto|manual\n");
            return;
        }
        const auto mode = (strcmp(m, "auto") == 0) ? ESP_RTL_SDR_GAIN_MODE_AUTO
                                                   : ESP_RTL_SDR_GAIN_MODE_MANUAL;
        const esp_err_t err = esp_rtl_sdr_set_tuner_gain_mode(s_dev[idx], mode);
        printf("[RTL%d] gainmode -> %s\n", idx, esp_rtl_sdr_err_to_name(err));
        return;
    }
    if (strcmp(cmd, "stream") == 0) {
        char *n = strtok(nullptr, " \t\r\n");
        char *act = strtok(nullptr, " \t\r\n");
        if (n == nullptr || act == nullptr) {
            printf("usage: rtl stream N|all start|stop\n");
            return;
        }
        const bool all = strcmp(n, "all") == 0;
        const bool do_start = strcmp(act, "start") == 0;
        static const uint32_t kDemoFreq[kMaxDev] = {99100000u, 101100000u, 103100000u};
        if (all) {
            for (int i = 0; i < kMaxDev; ++i) {
                if (do_start) {
                    (void)start_one(i, kDemoFreq[i], kDefaultSps);
                } else {
                    stop_one(i);
                }
            }
            return;
        }
        const int idx = atoi(n);
        if (do_start) {
            uint32_t freq = kDemoFreq[(idx >= 0 && idx < kMaxDev) ? idx : 0];
            uint32_t have = 0;
            if (valid_idx(idx)) {
                (void)esp_rtl_sdr_get_center_freq(s_dev[idx], &have);
                if (have != 0) {
                    freq = have;
                }
            }
            (void)start_one(idx, freq, kDefaultSps);
        } else {
            stop_one(idx);
        }
        return;
    }
    printf("unknown rtl command. try help\n");
}

static void watch_task(void *arg)
{
    (void)arg;
    while (true) {
        const int ms = s_watch_ms;
        if (ms > 0) {
            cmd_stats();
            vTaskDelay(pdMS_TO_TICKS(ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

extern "C" void app_main(void)
{
    (void)nvs_flash_init();
    printf("\n=== esp_rtl_sdr multi-receiver harness %s ===\n",
           esp_rtl_sdr_get_version_string());
    printf("hubs_kconfig=%d  (need DEVICE jumper + CH334 for 2-3 dongles)\n",
#if defined(CONFIG_USB_HOST_HUBS_SUPPORTED) && CONFIG_USB_HOST_HUBS_SUPPORTED
           1
#else
           0
#endif
    );
    print_help();

    for (int i = 0; i < kMaxDev; ++i) {
        esp_rtl_sdr_config_t cfg;
        esp_rtl_sdr_config_default(&cfg);
        cfg.event_cb = on_event;
        cfg.event_ctx = reinterpret_cast<void *>(static_cast<intptr_t>(i));
        cfg.bind_device_index = ESP_RTL_SDR_BIND_ANY;
        cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_CALLBACK;
        const esp_err_t err = esp_rtl_sdr_install(&cfg, &s_dev[i]);
        printf("install RTL%d -> %s\n", i, esp_rtl_sdr_err_to_name(err));
        if (err != ESP_OK) {
            s_dev[i] = nullptr;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1500));
    cmd_list();

    xTaskCreate(watch_task, "rtl_watch", 4096, nullptr, 5, nullptr);

    char line[160];
    while (true) {
        printf("rtl> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        handle_line(line);
    }
}
