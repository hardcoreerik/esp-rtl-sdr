/*
 * esp_rtl_sdr function tests. Hardware-in-the-loop: every test talks to real
 * dongles through the public API only. Results go through ft::Reporter.
 *
 * Conventions
 *  - A test that cannot run reports SKIP with a reason; it never passes silently.
 *  - A test leaves its dongle IDLE (StopGuard) so the next test starts clean.
 *  - Async setters (gain, AGC, bandwidth, bias) are judged by "accepted + the
 *    stream keeps flowing + no EVT_ERROR + state is not FAULT". The driver
 *    does not read registers back, so that is the strongest honest check.
 */

#include "ft_tests.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "esp_rtl_sdr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ft_expect.hpp"
#include "sdkconfig.h"

namespace {

constexpr int kMaxDev = CONFIG_FT_MAX_DEVICES;
constexpr uint32_t kFmHz = 99100000u; /* in range for every profile, no stimulus needed */
constexpr uint32_t kRate = ESP_RTL_SDR_RATE_2400K;
constexpr uint32_t kRatePct = 5; /* effective vs programmed sample rate tolerance */

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
void delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

struct Dev {
    esp_rtl_sdr_handle_t h = nullptr;
    esp_rtl_sdr_identity_t id{};
    const ft::Expect *exp = nullptr;
    const char *pname = "";
    bool present = false;
    std::atomic<uint32_t> iq_blocks{0};
    std::atomic<uint32_t> seq_gaps{0};
    std::atomic<uint32_t> last_seq{0};
    std::atomic<bool> have_seq{false};
    std::atomic<uint32_t> errors{0};
    std::atomic<uint32_t> disconnects{0};

    void reset_counters()
    {
        iq_blocks = 0;
        seq_gaps = 0;
        last_seq = 0;
        have_seq = false;
        errors = 0;
    }
};

Dev g_dev[3];
int g_installed = 0;
int g_present = 0;

void on_event(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    Dev *d = static_cast<Dev *>(ctx);
    if (d == nullptr) {
        return;
    }
    switch (ev) {
    case ESP_RTL_SDR_EVT_IQ_BLOCK: {
        const auto *iq = static_cast<const esp_rtl_sdr_iq_block_t *>(payload);
        d->iq_blocks++;
        if (iq != nullptr) {
            if (d->have_seq && iq->sequence != d->last_seq + 1u) {
                d->seq_gaps++;
            }
            d->last_seq = iq->sequence;
            d->have_seq = true;
        }
        break;
    }
    case ESP_RTL_SDR_EVT_ERROR:
        d->errors++;
        break;
    case ESP_RTL_SDR_EVT_DISCONNECTED:
        d->disconnects++;
        break;
    default:
        break;
    }
}

/* ---- small helpers ------------------------------------------------------ */

esp_rtl_sdr_stream_stats_t stats_of(Dev &d)
{
    esp_rtl_sdr_stream_stats_t st;
    esp_rtl_sdr_stream_stats_default(&st);
    (void)esp_rtl_sdr_get_stream_stats(d.h, &st);
    return st;
}

esp_rtl_sdr_metrics_t metrics_of(Dev &d)
{
    esp_rtl_sdr_metrics_t m;
    std::memset(&m, 0, sizeof(m));
    (void)esp_rtl_sdr_get_metrics(d.h, &m);
    return m;
}

esp_err_t start_dev(Dev &d, uint32_t hz = kFmHz, uint32_t sps = kRate)
{
    d.reset_counters();
    return esp_rtl_sdr_start_hz(d.h, hz, sps);
}

/** Stops the dongle when it leaves scope, whatever the test did. */
struct StopGuard {
    Dev &d;
    explicit StopGuard(Dev &dev) : d(dev) {}
    ~StopGuard()
    {
        if (d.h != nullptr && esp_rtl_sdr_get_state(d.h) != ESP_RTL_SDR_STATE_IDLE) {
            (void)esp_rtl_sdr_stop(d.h, 0);
        }
    }
};

/** True if bytes keep arriving over window_ms. */
bool flows(Dev &d, uint32_t window_ms = 300)
{
    const uint64_t b0 = stats_of(d).bytes_received;
    delay_ms(window_ms);
    return stats_of(d).bytes_received > b0;
}

/** Effective sample rate over a window, from byte counts and the real elapsed time. */
uint32_t measure_sps(Dev &d, uint32_t window_ms)
{
    const uint64_t b0 = stats_of(d).bytes_received;
    const int64_t t0 = esp_timer_get_time();
    delay_ms(window_ms);
    const uint64_t b1 = stats_of(d).bytes_received;
    const int64_t us = esp_timer_get_time() - t0;
    if (us <= 0) {
        return 0;
    }
    return static_cast<uint32_t>((b1 - b0) / 2u * 1000000ull / static_cast<uint64_t>(us));
}

bool wait_present(Dev &d, bool want, uint32_t timeout_ms)
{
    const uint32_t t0 = now_ms();
    while (now_ms() - t0 < timeout_ms) {
        esp_rtl_sdr_identity_t id;
        esp_rtl_sdr_identity_default(&id);
        if (d.h != nullptr && esp_rtl_sdr_get_identity(d.h, &id) == ESP_OK && id.present == want) {
            if (want) {
                d.id = id;
                d.exp = ft::find_expect(id.profile);
                d.pname = esp_rtl_sdr_profile_to_name(id.profile);
            }
            return true;
        }
        delay_ms(100);
    }
    return false;
}

esp_err_t install_dev(Dev &d, esp_rtl_sdr_delivery_mode_t mode)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_event;
    cfg.event_ctx = &d;
    cfg.bind_device_index = ESP_RTL_SDR_BIND_ANY;
    cfg.delivery_mode = mode;
    if (d.id.serial[0] != '\0') {
        std::strncpy(cfg.bind_serial, d.id.serial, sizeof(cfg.bind_serial) - 1);
    }
    return esp_rtl_sdr_install(&cfg, &d.h);
}

/** Uninstall and install again, keeping the same dongle (matched by serial). */
bool reinstall(Dev &d, esp_rtl_sdr_delivery_mode_t mode)
{
    (void)esp_rtl_sdr_uninstall(d.h);
    d.h = nullptr;
    delay_ms(300);
    if (install_dev(d, mode) != ESP_OK) {
        return false;
    }
    return wait_present(d, true, 6000);
}

const char *err_name(esp_err_t e) { return esp_rtl_sdr_err_to_name(e); }

/* ---- global tests ------------------------------------------------------- */

void t_version(ft::Reporter &r)
{
    ft::Case c(r, "env.version", -1, "", now_ms);
    c.check(esp_rtl_sdr_get_version() != 0, "get_version() is 0");
    const char *s = esp_rtl_sdr_get_version_string();
    c.check(s != nullptr && s[0] != '\0', "version string empty");
    c.note("driver %s", s != nullptr ? s : "?");
    c.finish();
}

void t_policy(ft::Reporter &r)
{
    ft::Case c(r, "policy.rates", -1, "", now_ms);
    struct Row { uint32_t sps; bool ok; };
    static const Row rows[] = {
        {225000, false}, {225001, true},  {300000, true},  {300001, false}, {900000, false},
        {900001, true},  {2400000, true}, {3200000, true}, {3200001, false}, {0, false},
    };
    for (const Row &row : rows) {
        c.check(esp_rtl_sdr_is_rate_supported(row.sps) == row.ok, "is_rate_supported(%u) != %d",
                static_cast<unsigned>(row.sps), static_cast<int>(row.ok));
    }
    uint32_t exact = 0;
    c.check(esp_rtl_sdr_quantize_sample_rate(2400000, &exact), "quantize(2.4M) rejected");
    c.check(ft::within_pct(exact, 2400000, 1), "quantize(2.4M)=%u", static_cast<unsigned>(exact));
    c.check(!esp_rtl_sdr_quantize_sample_rate(500000, &exact), "quantize(500k) accepted (gap)");
    uint32_t hz = 0;
    c.check(esp_rtl_sdr_normalize_frequency(kFmHz, &hz) && hz == kFmHz, "normalize(99.1M)");
    c.check(!esp_rtl_sdr_normalize_frequency(ESP_RTL_SDR_FREQ_MAX_HZ + 1u, &hz),
            "normalize accepted > max");
    c.finish();
}

void t_null_safety(ft::Reporter &r)
{
    ft::Case c(r, "api.null_safety", -1, "", now_ms);
    c.check(esp_rtl_sdr_uninstall(nullptr) == ESP_OK, "uninstall(NULL) != OK");
    c.check(esp_rtl_sdr_start(nullptr, nullptr) != ESP_OK, "start(NULL) returned OK");
    c.check(esp_rtl_sdr_start_hz(nullptr, kFmHz, kRate) != ESP_OK, "start_hz(NULL) returned OK");
    c.check(esp_rtl_sdr_retune_hz(nullptr, kFmHz) != ESP_OK, "retune(NULL) returned OK");
    c.check(esp_rtl_sdr_set_sample_rate(nullptr, kRate) != ESP_OK, "set_sample_rate(NULL) OK");
    c.check(esp_rtl_sdr_get_center_freq(nullptr, nullptr) != ESP_OK, "get_center_freq(NULL,NULL) OK");
    c.check(esp_rtl_sdr_config_validate(nullptr) != ESP_OK, "config_validate(NULL) OK");
    (void)esp_rtl_sdr_stop(nullptr, 0);
    (void)esp_rtl_sdr_get_state(nullptr);
    c.check(esp_rtl_sdr_get_profile(nullptr) == ESP_RTL_SDR_PROFILE_UNKNOWN, "get_profile(NULL)");
    c.check(esp_rtl_sdr_get_device_capabilities(nullptr) == 0, "get_device_capabilities(NULL)");
    c.finish();
}

void t_enumerate(ft::Reporter &r)
{
    ft::Case c(r, "env.enumerate", -1, "", now_ms);
    char list[150] = "";
    size_t used = 0;
    for (int i = 0; i < g_installed; ++i) {
        Dev &d = g_dev[i];
        if (!d.present) {
            continue;
        }
        const int n = std::snprintf(list + used, sizeof(list) - used, "%s%d:%s@%s", used ? " " : "",
                                    i, d.pname, d.id.usb_path);
        if (n > 0 && static_cast<size_t>(n) < sizeof(list) - used) {
            used += static_cast<size_t>(n);
        }
    }
    size_t bus = 0;
    (void)esp_rtl_sdr_usb_device_count(&bus);
    c.check(g_present >= CONFIG_FT_MIN_DEVICES, "%d dongle(s) present, need %d (usb devices on bus: %u)",
            g_present, CONFIG_FT_MIN_DEVICES, static_cast<unsigned>(bus));
    for (int i = 0; i < g_installed; ++i) {
        for (int j = i + 1; j < g_installed; ++j) {
            if (g_dev[i].present && g_dev[j].present) {
                c.check(g_dev[i].id.usb_addr != g_dev[j].id.usb_addr, "dev %d and %d share usb addr %u",
                        i, j, static_cast<unsigned>(g_dev[i].id.usb_addr));
            }
        }
    }
    /* A dongle on the bus that the driver did not accept is the generic/unknown case. */
    c.note("present=%d bus_devices=%u [%s]", g_present, static_cast<unsigned>(bus), list);
    c.finish();
}

/* ---- per-device tests --------------------------------------------------- */

void t_identity(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "dev.identity", i, d.pname, now_ms);
    c.check(d.id.profile != ESP_RTL_SDR_PROFILE_UNKNOWN, "profile UNKNOWN");
    c.check(d.id.usb_addr != 0, "usb_addr 0");
    c.check(d.id.high_speed, "not high-speed (480 Mbit/s) - check cable/hub");
    c.check(d.id.vid == ESP_RTL_SDR_USB_VID && d.id.pid == ESP_RTL_SDR_USB_PID, "vid:pid %04x:%04x",
            d.id.vid, d.id.pid);
    uint8_t li = 0xFF;
    c.check(esp_rtl_sdr_get_logical_index(d.h, &li) == ESP_OK && li < ESP_RTL_SDR_MAX_DEVICES,
            "logical index %u", static_cast<unsigned>(li));
    if (d.exp == nullptr && !c.failed()) {
        c.skip("no expectation table for profile %s (new dongle): add a row to ft_expect.hpp", d.pname);
        return;
    }
    c.note("%s %s serial=%s", d.id.manufacturer, d.id.product, d.id.serial);
    c.finish();
}

void t_caps(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    if (d.exp == nullptr) {
        ft::Case c(r, "dev.caps", i, d.pname, now_ms);
        c.skip("no expectation table for profile %s", d.pname);
        return;
    }
    ft::Case c(r, "dev.caps", i, d.pname, now_ms);
    const uint32_t caps = esp_rtl_sdr_get_device_capabilities(d.h);
    const uint32_t missing = d.exp->caps_required & ~caps;
    const uint32_t extra = d.exp->caps_forbidden & caps;
    c.check(missing == 0, "missing caps 0x%08x", static_cast<unsigned>(missing));
    c.check(extra == 0, "forbidden caps present 0x%08x", static_cast<unsigned>(extra));
    c.check((esp_rtl_sdr_get_capabilities() & caps) == caps, "device caps not a subset of library caps");
    c.note("caps=0x%08x", static_cast<unsigned>(caps));
    c.finish();
}

void t_contract_idle(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "contract.idle", i, d.pname, now_ms);
    StopGuard g(d);
    c.check(esp_rtl_sdr_get_state(d.h) == ESP_RTL_SDR_STATE_IDLE, "not IDLE before test");
    c.check(esp_rtl_sdr_retune_hz(d.h, kFmHz) == ESP_RTL_SDR_ERR_NOT_STREAMING,
            "retune while idle: %s", err_name(esp_rtl_sdr_retune_hz(d.h, kFmHz)));
    c.check(esp_rtl_sdr_stop(d.h, 0) == ESP_OK, "stop while idle not idempotent");
    c.check(esp_rtl_sdr_set_center_freq(d.h, 0) != ESP_OK, "set_center_freq(0) accepted");
    uint32_t hz = 0;
    c.check(esp_rtl_sdr_set_center_freq(d.h, kFmHz) == ESP_OK &&
                esp_rtl_sdr_get_center_freq(d.h, &hz) == ESP_OK && hz == kFmHz,
            "center freq store/readback (%u)", static_cast<unsigned>(hz));
    c.check(esp_rtl_sdr_set_sample_rate(d.h, 300001) == ESP_RTL_SDR_ERR_BAD_RATE,
            "set_sample_rate(300001) accepted");
    uint32_t sps = 0;
    c.check(esp_rtl_sdr_set_sample_rate(d.h, kRate) == ESP_OK &&
                esp_rtl_sdr_get_sample_rate(d.h, &sps) == ESP_OK && ft::within_pct(sps, kRate, 1),
            "sample rate store/readback (%u)", static_cast<unsigned>(sps));
    int ppm = 0;
    c.check(esp_rtl_sdr_set_freq_correction(d.h, ESP_RTL_SDR_PPM_MAX + 1) != ESP_OK, "ppm +201 accepted");
    c.check(esp_rtl_sdr_set_freq_correction(d.h, ESP_RTL_SDR_PPM_MIN - 1) != ESP_OK, "ppm -201 accepted");
    c.check(esp_rtl_sdr_set_freq_correction(d.h, 25) == ESP_OK &&
                esp_rtl_sdr_get_freq_correction(d.h, &ppm) == ESP_OK && ppm == 25,
            "ppm store/readback (%d)", ppm);
    (void)esp_rtl_sdr_set_freq_correction(d.h, 0);
    c.check(esp_rtl_sdr_apply_need(d.h, ESP_RTL_SDR_NEED_FM) == ESP_OK, "apply_need(FM)");
    c.finish();
}

void t_contract_streaming(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "contract.streaming", i, d.pname, now_ms);
    StopGuard g(d);
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    c.check(esp_rtl_sdr_get_state(d.h) == ESP_RTL_SDR_STATE_STREAMING, "state not STREAMING");
    c.check(esp_rtl_sdr_start_hz(d.h, kFmHz, kRate) == ESP_RTL_SDR_ERR_BUSY, "second start not BUSY");
    c.check(esp_rtl_sdr_set_sample_rate(d.h, 2000000) == ESP_RTL_SDR_ERR_BUSY,
            "set_sample_rate while streaming not BUSY");
    uint8_t buf[16];
    size_t got = 0;
    c.check(esp_rtl_sdr_read(d.h, buf, sizeof(buf), 0, &got) == ESP_RTL_SDR_ERR_UNSUPPORTED,
            "read() in CALLBACK mode not UNSUPPORTED");
    if (d.exp != nullptr) {
        if (d.exp->hf_direct_route) {
            uint32_t min_hz = 1;
            c.check(esp_rtl_sdr_set_hf_direct_min_hz(d.h, 24000000u) == ESP_OK &&
                        esp_rtl_sdr_get_hf_direct_min_hz(d.h, &min_hz) == ESP_OK && min_hz == 24000000u,
                    "hf_direct_min set/get (%u)", static_cast<unsigned>(min_hz));
            c.check(esp_rtl_sdr_set_hf_direct_min_hz(d.h, 0) == ESP_OK, "hf_direct_min restore");
        } else {
            c.check(esp_rtl_sdr_set_hf_direct_min_hz(d.h, 24000000u) == ESP_RTL_SDR_ERR_UNSUPPORTED,
                    "hf_direct_min not UNSUPPORTED on this profile");
        }
    }
    c.check(esp_rtl_sdr_stop(d.h, 0) == ESP_OK, "stop failed");
    c.check(esp_rtl_sdr_stop(d.h, 0) == ESP_OK, "second stop not idempotent");
    c.check(esp_rtl_sdr_get_state(d.h) == ESP_RTL_SDR_STATE_IDLE, "state not IDLE after stop");
    c.finish();
}

void t_lifecycle(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "lifecycle.cycles", i, d.pname, now_ms);
    StopGuard g(d);
    /* One warm-up cycle so lazily-created pools do not count as a leak. */
    c.check(start_dev(d) == ESP_OK, "warm-up start");
    delay_ms(200);
    c.check(esp_rtl_sdr_stop(d.h, 0) == ESP_OK, "warm-up stop");
    const uint32_t heap0 = esp_get_free_heap_size();
    int ok_cycles = 0;
    for (int n = 0; n < CONFIG_FT_LIFECYCLE_CYCLES; ++n) {
        const esp_err_t se = start_dev(d);
        if (!c.check(se == ESP_OK, "cycle %d start: %s", n, err_name(se))) {
            break;
        }
        if (!c.check(flows(d, 150), "cycle %d no data", n)) {
            (void)esp_rtl_sdr_stop(d.h, 0);
            break;
        }
        const esp_err_t pe = esp_rtl_sdr_stop(d.h, 0);
        if (!c.check(pe == ESP_OK, "cycle %d stop: %s", n, err_name(pe))) {
            break;
        }
        ++ok_cycles;
    }
    const uint32_t heap1 = esp_get_free_heap_size();
    const int32_t lost = static_cast<int32_t>(heap0) - static_cast<int32_t>(heap1);
    c.check(lost <= 4096, "heap fell %d bytes over %d cycles", static_cast<int>(lost), ok_cycles);
    c.note("%d cycles, heap delta %d", ok_cycles, static_cast<int>(lost));
    c.finish();
}

void t_stream_basic(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "stream.basic", i, d.pname, now_ms);
    StopGuard g(d);
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    delay_ms(1000);
    const esp_rtl_sdr_metrics_t m = metrics_of(d);
    c.check(d.iq_blocks > 0, "no EVT_IQ_BLOCK callbacks");
    c.check(m.bytes_total > 0, "no bytes");
    c.check(m.sample_min < m.sample_max, "IQ constant (min=%u max=%u): front end dead?",
            static_cast<unsigned>(m.sample_min), static_cast<unsigned>(m.sample_max));
    c.check(m.frequency_hz == kFmHz, "metrics freq %u", static_cast<unsigned>(m.frequency_hz));
    c.check(d.errors == 0, "%u EVT_ERROR", static_cast<unsigned>(d.errors.load()));
    c.note("bytes=%llu min=%u max=%u mean=%.1f", static_cast<unsigned long long>(m.bytes_total),
           static_cast<unsigned>(m.sample_min), static_cast<unsigned>(m.sample_max),
           static_cast<double>(m.sample_mean));
    c.finish();
}

void t_rate_sweep(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "stream.rate_sweep", i, d.pname, now_ms);
    StopGuard g(d);
    static const uint32_t rates[] = {250000, 960000, 1024000, 2048000, 2400000};
    char note[150] = "";
    size_t used = 0;
    for (uint32_t rate : rates) {
        uint32_t exact = 0;
        (void)esp_rtl_sdr_quantize_sample_rate(rate, &exact);
        const esp_err_t se = start_dev(d, kFmHz, rate);
        if (!c.check(se == ESP_OK, "start @%u: %s", static_cast<unsigned>(rate), err_name(se))) {
            continue;
        }
        uint32_t reported = 0;
        (void)esp_rtl_sdr_get_sample_rate(d.h, &reported);
        c.check(ft::within_pct(reported, exact, 1), "@%u reported rate %u", static_cast<unsigned>(rate),
                static_cast<unsigned>(reported));
        delay_ms(500);
        const uint32_t sps = measure_sps(d, 2000);
        c.check(ft::within_pct(sps, exact, kRatePct), "@%u effective %u S/s", static_cast<unsigned>(rate),
                static_cast<unsigned>(sps));
        const int n = std::snprintf(note + used, sizeof(note) - used, "%s%u>%u", used ? " " : "",
                                    static_cast<unsigned>(rate), static_cast<unsigned>(sps));
        if (n > 0 && static_cast<size_t>(n) < sizeof(note) - used) {
            used += static_cast<size_t>(n);
        }
        c.check(esp_rtl_sdr_stop(d.h, 0) == ESP_OK, "stop @%u", static_cast<unsigned>(rate));
    }
    /* 3.2 MS/s is the vendor maximum and drops are expected: only prove it starts. */
    const esp_err_t he = start_dev(d, kFmHz, ESP_RTL_SDR_RATE_3200K);
    c.check(he == ESP_OK, "start @3.2M: %s", err_name(he));
    if (he == ESP_OK) {
        delay_ms(500);
        const uint32_t sps = measure_sps(d, 1500);
        const int n = std::snprintf(note + used, sizeof(note) - used, " 3200000>%u(info)",
                                    static_cast<unsigned>(sps));
        (void)n;
        (void)esp_rtl_sdr_stop(d.h, 0);
    }
    c.note("%s", note);
    c.finish();
}

void t_integrity(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "stream.integrity", i, d.pname, now_ms);
    StopGuard g(d);
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    delay_ms(1000);
    const esp_rtl_sdr_stream_stats_t s0 = stats_of(d);
    const esp_rtl_sdr_metrics_t m0 = metrics_of(d);
    const uint32_t gaps0 = d.seq_gaps;
    const uint32_t sps = measure_sps(d, CONFIG_FT_SOAK_SECONDS * 1000u);
    const esp_rtl_sdr_stream_stats_t s1 = stats_of(d);
    const esp_rtl_sdr_metrics_t m1 = metrics_of(d);
    c.check(ft::within_pct(sps, kRate, kRatePct), "effective %u S/s", static_cast<unsigned>(sps));
    c.check(d.seq_gaps - gaps0 == 0, "%u IQ sequence gaps", static_cast<unsigned>(d.seq_gaps - gaps0));
    c.check(s1.usb_transfer_errors == s0.usb_transfer_errors, "%u USB transfer errors",
            static_cast<unsigned>(s1.usb_transfer_errors - s0.usb_transfer_errors));
    c.check(s1.slot_starve_blocks == s0.slot_starve_blocks, "%u slot-starve blocks",
            static_cast<unsigned>(s1.slot_starve_blocks - s0.slot_starve_blocks));
    c.check(m1.overruns == m0.overruns, "%u overruns", static_cast<unsigned>(m1.overruns - m0.overruns));
    c.check(d.errors == 0, "%u EVT_ERROR", static_cast<unsigned>(d.errors.load()));
    c.note("%us @%u S/s, queue_hw=%u", static_cast<unsigned>(CONFIG_FT_SOAK_SECONDS),
           static_cast<unsigned>(sps), static_cast<unsigned>(s1.queue_high_water));
    c.finish();
}

void t_retune(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "retune.sweep", i, d.pname, now_ms);
    StopGuard g(d);
    if (d.exp == nullptr) {
        c.skip("no expectation table for profile %s", d.pname);
        return;
    }
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    uint32_t pts[16];
    size_t n = 0;
    pts[n++] = 99100000u;
    pts[n++] = 162400000u;
    pts[n++] = 433920000u;
    pts[n++] = 1090000000u;
    const uint32_t caps = esp_rtl_sdr_get_device_capabilities(d.h);
    if ((caps & (ESP_RTL_SDR_CAP_HF_UPCONVERTER | ESP_RTL_SDR_CAP_DIRECT_SAMPLING)) != 0) {
        /* HF route and its edges: 24 MHz direct-sampling cutoff, 28.8 MHz upconverter LO. */
        pts[n++] = 10000000u;
        pts[n++] = 23999999u;
        pts[n++] = 24000000u;
        pts[n++] = 28799999u;
        pts[n++] = 28800000u;
        pts[n++] = 28800001u;
    }
    pts[n++] = 99100000u;
    unsigned done = 0;
    for (size_t k = 0; k < n; ++k) {
        const uint32_t f = pts[k];
        if (f < d.exp->rf_min_hz || f > d.exp->rf_max_hz) {
            continue;
        }
        const uint32_t errs0 = d.errors;
        const esp_err_t e = esp_rtl_sdr_retune_hz(d.h, f);
        if (!c.check(e == ESP_OK, "retune %u: %s", static_cast<unsigned>(f), err_name(e))) {
            continue;
        }
        uint32_t got = 0;
        (void)esp_rtl_sdr_get_center_freq(d.h, &got);
        c.check(got == f, "center freq %u after retune to %u", static_cast<unsigned>(got),
                static_cast<unsigned>(f));
        c.check(flows(d, 300), "no data after retune to %u", static_cast<unsigned>(f));
        c.check(esp_rtl_sdr_get_state(d.h) == ESP_RTL_SDR_STATE_STREAMING, "left STREAMING at %u",
                static_cast<unsigned>(f));
        c.check(d.errors == errs0, "EVT_ERROR after retune to %u", static_cast<unsigned>(f));
        ++done;
    }
    /* Out-of-range requests must be refused and must not disturb the stream. */
    c.check(esp_rtl_sdr_retune_hz(d.h, 0) != ESP_OK, "retune(0) accepted");
    c.check(esp_rtl_sdr_retune_hz(d.h, d.exp->rf_max_hz + 1u) == ESP_RTL_SDR_ERR_BAD_FREQ,
            "retune above max accepted");
    if (d.exp->rf_min_hz > ESP_RTL_SDR_FREQ_MIN_HZ) {
        c.check(esp_rtl_sdr_retune_hz(d.h, d.exp->rf_min_hz - 1u) == ESP_RTL_SDR_ERR_BAD_FREQ,
                "retune below profile min accepted");
    }
    c.check(flows(d, 300) && esp_rtl_sdr_get_state(d.h) == ESP_RTL_SDR_STATE_STREAMING,
            "stream disturbed by refused retune");
    c.note("%u retune points", done);
    c.finish();
}

void t_gain(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "controls.gain", i, d.pname, now_ms);
    StopGuard g(d);
    const uint32_t caps = esp_rtl_sdr_get_device_capabilities(d.h);
    if ((caps & ESP_RTL_SDR_CAP_GAIN) == 0) {
        c.skip("profile has no CAP_GAIN");
        return;
    }
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    int gains[48];
    size_t count = 0;
    c.check(esp_rtl_sdr_get_tuner_gains(d.h, gains, 48, &count) == ESP_OK && count > 0, "no gain ladder");
    if (d.exp != nullptr) {
        c.check(count == d.exp->gain_steps, "ladder has %u steps, expected %u", static_cast<unsigned>(count),
                static_cast<unsigned>(d.exp->gain_steps));
    }
    if (count > 0 && count <= 48) {
        const int picks[3] = {gains[0], gains[count / 2], gains[count - 1]};
        for (int pick : picks) {
            const uint32_t errs0 = d.errors;
            const esp_err_t e = esp_rtl_sdr_set_tuner_gain(d.h, pick);
            c.check(e == ESP_OK, "set_tuner_gain(%d): %s", pick, err_name(e));
            int back = -1;
            c.check(esp_rtl_sdr_get_tuner_gain(d.h, &back) == ESP_OK && back == pick, "gain readback %d != %d",
                    back, pick);
            delay_ms(200);
            c.check(flows(d, 300), "no data at gain %d", pick);
            c.check(d.errors == errs0, "EVT_ERROR at gain %d", pick);
        }
    }
    if ((caps & ESP_RTL_SDR_CAP_GAIN_AUTO) != 0) {
        const uint32_t errs0 = d.errors;
        c.check(esp_rtl_sdr_set_tuner_gain_mode(d.h, ESP_RTL_SDR_GAIN_MODE_AUTO) == ESP_OK, "set AUTO");
        esp_rtl_sdr_gain_mode_t mode = ESP_RTL_SDR_GAIN_MODE_MANUAL;
        c.check(esp_rtl_sdr_get_tuner_gain_mode(d.h, &mode) == ESP_OK && mode == ESP_RTL_SDR_GAIN_MODE_AUTO,
                "AUTO readback");
        delay_ms(200);
        c.check(flows(d, 300), "no data in AUTO");
        c.check(esp_rtl_sdr_set_tuner_gain_mode(d.h, ESP_RTL_SDR_GAIN_MODE_MANUAL) == ESP_OK, "set MANUAL");
        delay_ms(200);
        c.check(flows(d, 300), "no data back in MANUAL");
        c.check(d.errors == errs0, "EVT_ERROR around AUTO/MANUAL");
    }
    c.check(esp_rtl_sdr_get_state(d.h) != ESP_RTL_SDR_STATE_FAULT, "FAULT");
    c.note("ladder=%u", static_cast<unsigned>(count));
    c.finish();
}

void t_rtl_agc(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "controls.rtl_agc", i, d.pname, now_ms);
    StopGuard g(d);
    if ((esp_rtl_sdr_get_device_capabilities(d.h) & ESP_RTL_SDR_CAP_RTL_AGC) == 0) {
        c.skip("profile has no CAP_RTL_AGC");
        return;
    }
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    for (bool on : {true, false}) {
        const uint32_t errs0 = d.errors;
        bool back = !on;
        c.check(esp_rtl_sdr_set_rtl_agc(d.h, on) == ESP_OK, "set_rtl_agc(%d)", on);
        c.check(esp_rtl_sdr_get_rtl_agc(d.h, &back) == ESP_OK && back == on, "rtl_agc readback");
        delay_ms(200);
        c.check(flows(d, 300), "no data with rtl_agc=%d", on);
        c.check(d.errors == errs0, "EVT_ERROR with rtl_agc=%d", on);
    }
    c.finish();
}

void t_bandwidth(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "controls.bandwidth", i, d.pname, now_ms);
    StopGuard g(d);
    if ((esp_rtl_sdr_get_device_capabilities(d.h) & ESP_RTL_SDR_CAP_TUNER_BANDWIDTH) == 0) {
        c.skip("profile has no CAP_TUNER_BANDWIDTH");
        return;
    }
    if (!c.check(start_dev(d, kFmHz, ESP_RTL_SDR_RATE_2400K) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    uint32_t list[16];
    size_t count = 0;
    c.check(esp_rtl_sdr_get_tuner_bandwidths(d.h, list, 16, &count) == ESP_OK, "get_tuner_bandwidths");
    if (count == 0 || count > 16) {
        c.skip("no tuner bandwidth list on this route");
        return;
    }
    for (size_t k = 0; k < count; ++k) {
        const uint32_t hz = list[k];
        const uint32_t errs0 = d.errors;
        const esp_err_t e = esp_rtl_sdr_set_tuner_bandwidth(d.h, hz);
        if (!c.check(e == ESP_OK, "set_tuner_bandwidth(%u): %s", static_cast<unsigned>(hz), err_name(e))) {
            continue;
        }
        uint32_t req = 0, applied = 0;
        const uint32_t t0 = now_ms();
        /* 0 = automatic: the applied width is whatever the driver picked, so only
         * a fixed width is required to read back as applied. */
        while (now_ms() - t0 < 1500) {
            (void)esp_rtl_sdr_get_tuner_bandwidth_state(d.h, &req, &applied);
            if (applied != 0 && (hz == 0 || applied == hz)) {
                break;
            }
            delay_ms(50);
        }
        c.check(req == hz, "bandwidth requested readback %u != %u", static_cast<unsigned>(req),
                static_cast<unsigned>(hz));
        if (hz != 0) {
            c.check(applied == hz, "bandwidth %u never applied (applied=%u)", static_cast<unsigned>(hz),
                    static_cast<unsigned>(applied));
        }
        c.check(flows(d, 300), "no data at bandwidth %u", static_cast<unsigned>(hz));
        c.check(d.errors == errs0, "EVT_ERROR at bandwidth %u", static_cast<unsigned>(hz));
        c.check(esp_rtl_sdr_get_state(d.h) == ESP_RTL_SDR_STATE_STREAMING, "left STREAMING at bandwidth %u",
                static_cast<unsigned>(hz));
    }
    /* Leave the tuner in automatic width for the tests that follow. */
    (void)esp_rtl_sdr_set_tuner_bandwidth(d.h, 0);
    delay_ms(300);
    c.note("%u widths", static_cast<unsigned>(count));
    c.finish();
}

void t_bias(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "controls.bias_tee", i, d.pname, now_ms);
    StopGuard g(d);
    if ((esp_rtl_sdr_get_device_capabilities(d.h) & ESP_RTL_SDR_CAP_BIAS_TEE) == 0) {
        c.skip("profile has no CAP_BIAS_TEE");
        return;
    }
#if !CONFIG_FT_ALLOW_BIAS
    c.skip("disabled (CONFIG_FT_ALLOW_BIAS=n): bias tee puts DC on the antenna port");
    return;
#else
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    for (bool on : {true, false}) {
        const uint32_t errs0 = d.errors;
        bool back = !on;
        c.check(esp_rtl_sdr_set_bias_tee(d.h, on) == ESP_OK, "set_bias_tee(%d)", on);
        c.check(esp_rtl_sdr_get_bias_tee(d.h, &back) == ESP_OK && back == on, "bias readback");
        delay_ms(200);
        c.check(flows(d, 300), "no data with bias=%d", on);
        c.check(d.errors == errs0, "EVT_ERROR with bias=%d", on);
    }
    (void)esp_rtl_sdr_set_bias_tee(d.h, false);
    c.finish();
#endif
}

void t_read_sync(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "stream.read_sync", i, d.pname, now_ms);
    StopGuard g(d);
    if ((esp_rtl_sdr_get_device_capabilities(d.h) & ESP_RTL_SDR_CAP_SYNC_READ) == 0) {
        c.skip("profile has no CAP_SYNC_READ");
        return;
    }
    /* The suite installs CALLBACK-only handles; read() needs READ or BOTH. */
    if (!c.check(reinstall(d, ESP_RTL_SDR_DELIVERY_READ), "reinstall in READ mode (same dongle)")) {
        (void)reinstall(d, ESP_RTL_SDR_DELIVERY_CALLBACK);
        c.finish();
        return;
    }
    if (c.check(start_dev(d) == ESP_OK, "start failed")) {
        static uint8_t buf[16384];
        size_t total = 0;
        uint8_t lo = 255, hi = 0;
        const uint32_t t0 = now_ms();
        while (now_ms() - t0 < 1500) {
            size_t got = 0;
            const esp_err_t e = esp_rtl_sdr_read(d.h, buf, sizeof(buf), 500, &got);
            if (e != ESP_OK) {
                c.check(false, "read: %s", err_name(e));
                break;
            }
            for (size_t k = 0; k < got; ++k) {
                if (buf[k] < lo) lo = buf[k];
                if (buf[k] > hi) hi = buf[k];
            }
            total += got;
        }
        c.check(total >= 100000, "only %u bytes read in 1.5 s", static_cast<unsigned>(total));
        c.check(lo < hi, "read data constant (%u)", static_cast<unsigned>(lo));
        c.check(stats_of(d).bytes_consumed > 0, "bytes_consumed not counted");
        c.check(d.iq_blocks == 0, "EVT_IQ_BLOCK delivered in READ mode");
        c.note("%u bytes, min=%u max=%u", static_cast<unsigned>(total), static_cast<unsigned>(lo),
               static_cast<unsigned>(hi));
        (void)esp_rtl_sdr_stop(d.h, 0);
    }
    c.check(reinstall(d, ESP_RTL_SDR_DELIVERY_CALLBACK), "reinstall back to CALLBACK mode");
    c.finish();
}

void t_health(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "health.report", i, d.pname, now_ms);
    StopGuard g(d);
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    delay_ms(1500);
    esp_rtl_sdr_health_info_t h;
    std::memset(&h, 0, sizeof(h));
    c.check(esp_rtl_sdr_get_health(d.h, &h) == ESP_OK, "get_health");
    c.check(h.overall != ESP_RTL_SDR_HEALTH_UNKNOWN, "overall health UNKNOWN while streaming");
    c.check(h.usb != ESP_RTL_SDR_HEALTH_USB_STARVING, "USB starving (%s)", h.advice);
    c.check(h.overall != ESP_RTL_SDR_HEALTH_APP_TOO_SLOW, "app too slow (%s)", h.advice);
    /* RF_WEAK / RF_CLIPPING are properties of the antenna, not the driver. */
    c.note("overall=%d usb=%d rf=%d eff=%.2f", static_cast<int>(h.overall), static_cast<int>(h.usb),
           static_cast<int>(h.rf), static_cast<double>(h.efficiency));
    c.finish();
}

void t_passport(ft::Reporter &r, int i)
{
    Dev &d = g_dev[i];
    ft::Case c(r, "passport.probe", i, d.pname, now_ms);
    StopGuard g(d);
    if ((esp_rtl_sdr_get_device_capabilities(d.h) & ESP_RTL_SDR_CAP_PASSPORT) == 0) {
        c.skip("profile has no CAP_PASSPORT");
        return;
    }
#if !CONFIG_FT_RUN_PASSPORT
    c.skip("disabled (CONFIG_FT_RUN_PASSPORT=n): slow");
    return;
#else
    esp_rtl_sdr_passport_opts_t opts;
    esp_rtl_sdr_passport_opts_default(&opts);
    static esp_rtl_sdr_rate_passport_t pass;
    std::memset(&pass, 0, sizeof(pass));
    pass.struct_size = sizeof(pass);
    const esp_err_t e = esp_rtl_sdr_probe_rates(d.h, &opts, &pass);
    c.check(e == ESP_OK, "probe_rates: %s", err_name(e));
    c.check(pass.valid && pass.entry_count > 0, "passport empty");
    c.check(pass.best_stable_sps >= 960000u, "best stable rate %u", static_cast<unsigned>(pass.best_stable_sps));
    c.note("best_stable=%u max_tried=%u", static_cast<unsigned>(pass.best_stable_sps),
           static_cast<unsigned>(pass.max_tried_sps));
    c.finish();
#endif
}

/* ---- multi-device ------------------------------------------------------- */

int live_devices(Dev *out[], int max)
{
    int n = 0;
    for (int i = 0; i < g_installed && n < max; ++i) {
        if (g_dev[i].present) {
            out[n++] = &g_dev[i];
        }
    }
    return n;
}

void stop_all(Dev *live[], int n)
{
    for (int k = 0; k < n; ++k) {
        (void)esp_rtl_sdr_stop(live[k]->h, 0);
    }
}

void t_multi(ft::Reporter &r)
{
    Dev *live[3];
    const int n = live_devices(live, 3);
    if (n < 2) {
        for (const char *name : {"multi.concurrent", "multi.retune_isolation", "multi.stop_isolation"}) {
            ft::Case c(r, name, -1, "", now_ms);
            c.skip("needs >= 2 dongles, have %d", n);
        }
        return;
    }
    char mix[64] = "";
    {
        size_t used = 0;
        for (int k = 0; k < n; ++k) {
            const int w = std::snprintf(mix + used, sizeof(mix) - used, "%s%s", used ? "+" : "",
                                        live[k]->pname);
            if (w > 0 && static_cast<size_t>(w) < sizeof(mix) - used) {
                used += static_cast<size_t>(w);
            }
        }
    }

    /* 1. All dongles streaming at once for the soak window. */
    bool all_started = true;
    {
        ft::Case c(r, "multi.concurrent", -1, mix, now_ms);
        for (int k = 0; k < n; ++k) {
            const esp_err_t e = start_dev(*live[k], kFmHz + static_cast<uint32_t>(k) * 2000000u, kRate);
            if (!c.check(e == ESP_OK, "dev %d start: %s", k, err_name(e))) {
                all_started = false;
            }
        }
        if (all_started) {
            delay_ms(1000);
            esp_rtl_sdr_stream_stats_t s0[3];
            uint32_t gaps0[3];
            for (int k = 0; k < n; ++k) {
                s0[k] = stats_of(*live[k]);
                gaps0[k] = live[k]->seq_gaps;
            }
            const int64_t t0 = esp_timer_get_time();
            delay_ms(CONFIG_FT_SOAK_SECONDS * 1000u);
            const int64_t us = esp_timer_get_time() - t0;
            for (int k = 0; k < n; ++k) {
                const esp_rtl_sdr_stream_stats_t s1 = stats_of(*live[k]);
                const uint32_t sps = static_cast<uint32_t>((s1.bytes_received - s0[k].bytes_received) / 2u *
                                                           1000000ull / static_cast<uint64_t>(us));
                c.check(ft::within_pct(sps, kRate, kRatePct), "dev %d effective %u S/s", k,
                        static_cast<unsigned>(sps));
                c.check(live[k]->seq_gaps - gaps0[k] == 0, "dev %d: %u sequence gaps", k,
                        static_cast<unsigned>(live[k]->seq_gaps - gaps0[k]));
                c.check(s1.usb_transfer_errors == s0[k].usb_transfer_errors, "dev %d USB errors", k);
                c.check(s1.slot_starve_blocks == s0[k].slot_starve_blocks, "dev %d slot starve", k);
                c.check(live[k]->errors == 0, "dev %d EVT_ERROR", k);
            }
            esp_rtl_sdr_hub_stats_t hub;
            esp_rtl_sdr_hub_stats_default(&hub);
            (void)esp_rtl_sdr_get_hub_stats(live[0]->h, &hub);
            c.check(hub.claimed_rtl_count == static_cast<uint32_t>(n), "hub claimed %u, expected %d",
                    static_cast<unsigned>(hub.claimed_rtl_count), n);
            c.check(hub.claim_conflicts == 0, "%u claim conflicts", static_cast<unsigned>(hub.claim_conflicts));
            c.note("%d dongles x %us", n, static_cast<unsigned>(CONFIG_FT_SOAK_SECONDS));
        }
        c.finish();
    }
    if (!all_started) {
        stop_all(live, n);
        for (const char *name : {"multi.retune_isolation", "multi.stop_isolation"}) {
            ft::Case c(r, name, -1, mix, now_ms);
            c.skip("concurrent start failed");
        }
        return;
    }

    /* 2. Retune one dongle while the others keep streaming (Gate 6). */
    {
        ft::Case c(r, "multi.retune_isolation", -1, mix, now_ms);
        Dev &mover = *live[1];
        uint32_t gaps0[3], errs0[3];
        esp_rtl_sdr_stream_stats_t s0[3];
        for (int k = 0; k < n; ++k) {
            gaps0[k] = live[k]->seq_gaps;
            errs0[k] = live[k]->errors;
            s0[k] = stats_of(*live[k]);
        }
        const int64_t t0 = esp_timer_get_time();
        static const uint32_t hops[] = {101100000u, 107900000u, 88100000u, 162400000u, 101100000u};
        for (uint32_t f : hops) {
            const esp_err_t e = esp_rtl_sdr_retune_hz(mover.h, f);
            c.check(e == ESP_OK, "retune %u: %s", static_cast<unsigned>(f), err_name(e));
            delay_ms(300);
        }
        const int64_t us = esp_timer_get_time() - t0;
        for (int k = 0; k < n; ++k) {
            const esp_rtl_sdr_stream_stats_t s1 = stats_of(*live[k]);
            const uint32_t sps = static_cast<uint32_t>((s1.bytes_received - s0[k].bytes_received) / 2u *
                                                       1000000ull / static_cast<uint64_t>(us));
            if (live[k] == &mover) {
                c.check(sps > 0, "mover stopped streaming");
                continue;
            }
            /* Others only lose time to the shared USB bus, never to the retune itself. */
            c.check(ft::within_pct(sps, kRate, 10), "dev %d effective %u S/s during retune", k,
                    static_cast<unsigned>(sps));
            c.check(live[k]->seq_gaps - gaps0[k] == 0, "dev %d: %u sequence gaps during retune", k,
                    static_cast<unsigned>(live[k]->seq_gaps - gaps0[k]));
            c.check(live[k]->errors == errs0[k], "dev %d EVT_ERROR during retune", k);
        }
        c.finish();
    }

    /* 3. Stop one dongle while the others keep streaming, then bring it back. */
    {
        ft::Case c(r, "multi.stop_isolation", -1, mix, now_ms);
        Dev &victim = *live[0];
        c.check(esp_rtl_sdr_stop(victim.h, 0) == ESP_OK, "stop dev 0");
        for (int k = 1; k < n; ++k) {
            const uint32_t errs0 = live[k]->errors;
            const uint32_t sps = measure_sps(*live[k], 1500);
            c.check(ft::within_pct(sps, kRate, 10), "dev %d effective %u S/s after dev 0 stopped", k,
                    static_cast<unsigned>(sps));
            c.check(live[k]->errors == errs0, "dev %d EVT_ERROR", k);
        }
        const esp_err_t e = start_dev(victim);
        c.check(e == ESP_OK, "restart dev 0: %s", err_name(e));
        if (e == ESP_OK) {
            c.check(flows(victim, 500), "dev 0 no data after restart");
        }
        c.finish();
    }
    stop_all(live, n);
}

/* ---- operator-assisted hotplug ------------------------------------------ */

void t_hotplug(ft::Reporter &r)
{
#if !CONFIG_FT_INTERACTIVE
    ft::Case c(r, "hotplug.unplug_replug", -1, "", now_ms);
    c.skip("disabled (CONFIG_FT_INTERACTIVE=n): needs an operator");
#else
    Dev *live[3];
    if (live_devices(live, 3) < 1) {
        ft::Case c(r, "hotplug.unplug_replug", -1, "", now_ms);
        c.skip("no dongle");
        return;
    }
    Dev &d = *live[0];
    ft::Case c(r, "hotplug.unplug_replug", 0, d.pname, now_ms);
    StopGuard g(d);
    if (!c.check(start_dev(d) == ESP_OK, "start failed")) {
        c.finish();
        return;
    }
    d.disconnects = 0;
    std::printf("FT_PROMPT unplug dongle 0 (%s) within 30 s\n", d.id.usb_path);
    c.check(wait_present(d, false, 30000), "unplug not detected");
    c.check(d.disconnects > 0, "no EVT_DISCONNECTED");
    c.check(esp_rtl_sdr_get_state(d.h) != ESP_RTL_SDR_STATE_STREAMING, "still STREAMING after unplug");
    (void)esp_rtl_sdr_stop(d.h, 0);
    std::printf("FT_PROMPT replug the same dongle within 30 s\n");
    c.check(wait_present(d, true, 30000), "replug not detected");
    (void)esp_rtl_sdr_reset(d.h);
    if (!c.failed()) {
        c.check(start_dev(d) == ESP_OK, "restart after replug failed");
        c.check(flows(d, 500), "no data after replug");
        c.check(esp_rtl_sdr_stop(d.h, 0) == ESP_OK, "stop after replug");
    }
    c.finish();
#endif
}

} // namespace

int ft_devices_present(void) { return g_present; }

void ft_run_all(ft::Reporter &r)
{
    g_installed = 0;
    g_present = 0;
    for (int i = 0; i < kMaxDev; ++i) {
        Dev &d = g_dev[i];
        d.h = nullptr;
        d.exp = nullptr;
        d.pname = "";
        d.present = false;
        d.reset_counters();
        d.disconnects = 0;
        esp_rtl_sdr_identity_default(&d.id);
        const esp_err_t e = install_dev(d, ESP_RTL_SDR_DELIVERY_CALLBACK);
        if (e != ESP_OK) {
            ft::Case c(r, "env.install", i, "", now_ms);
            c.check(false, "install handle %d: %s", i, err_name(e));
            c.finish();
            d.h = nullptr;
            continue;
        }
        ++g_installed;
    }
    delay_ms(CONFIG_FT_BOOT_DELAY_MS);
    for (int i = 0; i < kMaxDev; ++i) {
        Dev &d = g_dev[i];
        if (d.h != nullptr && wait_present(d, true, 500)) {
            d.present = true;
            ++g_present;
        }
    }

    t_version(r);
    t_policy(r);
    t_null_safety(r);
    t_enumerate(r);

    for (int i = 0; i < kMaxDev; ++i) {
        Dev &d = g_dev[i];
        if (!d.present) {
            continue;
        }
        t_identity(r, i);
        t_caps(r, i);
        t_contract_idle(r, i);
        t_contract_streaming(r, i);
        t_lifecycle(r, i);
        t_stream_basic(r, i);
        t_rate_sweep(r, i);
        t_integrity(r, i);
        t_retune(r, i);
        t_gain(r, i);
        t_rtl_agc(r, i);
        t_bandwidth(r, i);
        t_bias(r, i);
        t_read_sync(r, i);
        t_health(r, i);
        t_passport(r, i);
    }
    t_multi(r);
    t_hotplug(r);

    for (int i = 0; i < kMaxDev; ++i) {
        if (g_dev[i].h != nullptr) {
            (void)esp_rtl_sdr_uninstall(g_dev[i].h);
            g_dev[i].h = nullptr;
        }
    }
}
