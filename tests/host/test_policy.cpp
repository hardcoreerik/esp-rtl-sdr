/*
 * Host unit tests for esp_rtl_sdr pure policy (no USB / FreeRTOS / IDF).
 * TheOrc-style: claims about rates/config/version must have automated proof.
 *
 * Build & run: tests/scripts/run_host_tests.ps1  (or .sh)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "esp_rtl_sdr.h"
#include "measured_gain_bias_v4.hpp"
#include "reentrancy.hpp"

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_U(a, b)                                                      \
    do {                                                                       \
        const auto _a = (a);                                                   \
        const auto _b = (b);                                                   \
        if (_a != _b) {                                                        \
            std::printf("FAIL %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, \
                        #a, (unsigned)_a, #b, (unsigned)_b);                   \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

#define EXPECT_EQ_I(a, b)                                                      \
    do {                                                                       \
        const auto _a = (a);                                                   \
        const auto _b = (b);                                                   \
        if (_a != _b) {                                                        \
            std::printf("FAIL %s:%d: %s (%d) != %s (%d)\n", __FILE__, __LINE__, \
                        #a, (int)_a, #b, (int)_b);                             \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

#define EXPECT_STREQ(a, b)                                                     \
    do {                                                                       \
        const char *_a = (a);                                                  \
        const char *_b = (b);                                                  \
        if (_a == nullptr || _b == nullptr || std::strcmp(_a, _b) != 0) {     \
            std::printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,  \
                        _a ? _a : "(null)", _b ? _b : "(null)");               \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

static void test_version(void)
{
    const char *vs = esp_rtl_sdr_get_version_string();
    EXPECT_TRUE(vs != nullptr);
    EXPECT_TRUE(vs[0] != '\0');
    EXPECT_STREQ(vs, ESP_RTL_SDR_VERSION_STRING);

    char expect[32];
    std::snprintf(expect, sizeof(expect), "%u.%u.%u", ESP_RTL_SDR_VERSION_MAJOR,
                  ESP_RTL_SDR_VERSION_MINOR, ESP_RTL_SDR_VERSION_PATCH);
    EXPECT_STREQ(vs, expect);

    const uint32_t packed = esp_rtl_sdr_get_version();
    EXPECT_EQ_U((packed >> 16) & 0xff, ESP_RTL_SDR_VERSION_MAJOR);
    EXPECT_EQ_U((packed >> 8) & 0xff, ESP_RTL_SDR_VERSION_MINOR);
    EXPECT_EQ_U(packed & 0xff, ESP_RTL_SDR_VERSION_PATCH);

    EXPECT_EQ_U(ESP_RTL_SDR_VERSION_NUMBER,
                ESP_RTL_SDR_VERSION_MAJOR * 10000u + ESP_RTL_SDR_VERSION_MINOR * 100u +
                    ESP_RTL_SDR_VERSION_PATCH);
}

static void test_capabilities(void)
{
    const uint32_t c = esp_rtl_sdr_get_capabilities();
    const uint32_t need_on = ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
                             ESP_RTL_SDR_CAP_METRICS | ESP_RTL_SDR_CAP_CUSTOM_HZ |
                             ESP_RTL_SDR_CAP_HOTPLUG | ESP_RTL_SDR_CAP_FREQ_CORRECTION |
                             ESP_RTL_SDR_CAP_MULTI_DEVICE | ESP_RTL_SDR_CAP_SYNC_READ |
                             ESP_RTL_SDR_CAP_CONTINUOUS_RATE | ESP_RTL_SDR_CAP_NEED |
                             ESP_RTL_SDR_CAP_HEALTH | ESP_RTL_SDR_CAP_PASSPORT |
                             ESP_RTL_SDR_CAP_DELIVERY_MODE;
    EXPECT_EQ_U(c & need_on, need_on);

    /* Measured Blog V4 lab 2026-08-12 — CAP_GAIN / CAP_BIAS_TEE on */
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_BIAS_TEE) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_GAIN) != 0);
    /* 0.7.7 HF upconverter path */
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_HF_UPCONVERTER) != 0);
    /* 0.7.8 measured Tuner AUTO + RTL digital AGC */
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_GAIN_AUTO) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_RTL_AGC) != 0);
    /* Still reserved */
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) == 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_IQ_ACQUIRE) == 0);
}

static void test_measured_agc_tables(void)
{
    /* Tuner AUTO trio is not a manual ladder step. */
    EXPECT_EQ_U(kMeasuredV4TunerAgcReg05, 0xe8);
    EXPECT_EQ_U(kMeasuredV4TunerAgcReg07, 0x78);
    EXPECT_EQ_U(kMeasuredV4TunerAgcReg0c, 0x6b);
    EXPECT_TRUE(kMeasuredV4TunerAgcReg0c != kMeasuredV4GainReg0c);

    bool auto_matches_manual = false;
    for (size_t i = 0; i < kMeasuredV4GainStepCount; ++i) {
        if (kMeasuredV4GainSteps[i].reg05 == kMeasuredV4TunerAgcReg05 &&
            kMeasuredV4GainSteps[i].reg07 == kMeasuredV4TunerAgcReg07) {
            auto_matches_manual = true;
        }
    }
    EXPECT_TRUE(!auto_matches_manual);

    /* 29.7 dB parked UI during capture is still on the manual ladder. */
    const size_t i297 = measured_v4_nearest_gain_index(297);
    EXPECT_EQ_I(kMeasuredV4GainSteps[i297].tenth_db, 297);
    EXPECT_EQ_U(kMeasuredV4GainSteps[i297].reg05, 0xf7);
    EXPECT_EQ_U(kMeasuredV4GainSteps[i297].reg07, 0x67);

    EXPECT_EQ_U(kMeasuredV4RtlAgcWvalue, 0x1920);
    EXPECT_EQ_U(kMeasuredV4RtlAgcWindex, 0x0010);
    EXPECT_EQ_U(kMeasuredV4RtlAgcOn, 0x25);
    EXPECT_EQ_U(kMeasuredV4RtlAgcOff, 0x05);
    EXPECT_TRUE(kMeasuredV4RtlAgcOn != kMeasuredV4RtlAgcOff);

    const RtlControlRecord ir = measured_v4_ir_reg_write(0x05, kMeasuredV4TunerAgcReg05);
    EXPECT_EQ_U(ir.value, 0x0074);
    EXPECT_EQ_U(ir.index, 0x0610);
    EXPECT_EQ_U(ir.request_type, 0x40);
    EXPECT_EQ_U(ir.length, 2);
    EXPECT_EQ_U(ir.data[0], 0x05);
    EXPECT_EQ_U(ir.data[1], 0xe8);

    const RtlControlRecord dem =
        measured_v4_demod_reg_write(kMeasuredV4RtlAgcReg, kMeasuredV4RtlAgcOn);
    EXPECT_EQ_U(dem.value, 0x1920);
    EXPECT_EQ_U(dem.index, 0x0010);
    EXPECT_EQ_U(dem.length, 1);
    EXPECT_EQ_U(dem.data[0], 0x25);
}

static void test_callback_reentry_is_task_local(void)
{
    const void *delivery = reinterpret_cast<const void *>(0x1000);
    const void *app = reinterpret_cast<const void *>(0x2000);

    EXPECT_TRUE(!esp_rtl_sdr_caller_is_event_callback(0, delivery, app));
    EXPECT_TRUE(!esp_rtl_sdr_caller_is_event_callback(1, nullptr, app));
    EXPECT_TRUE(!esp_rtl_sdr_caller_is_event_callback(1, delivery, nullptr));
    /* Delivery task emitting: app task is NOT reentrant. */
    EXPECT_TRUE(!esp_rtl_sdr_caller_is_event_callback(1, delivery, app));
    /* Callback task calling a setter: IS reentrant. */
    EXPECT_TRUE(esp_rtl_sdr_caller_is_event_callback(1, delivery, delivery));
    EXPECT_TRUE(esp_rtl_sdr_caller_is_event_callback(2, app, app));
}

static void test_rate_windows(void)
{
    /* Named + continuous */
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_250K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_256K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_960K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_1024K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_1800K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_2048K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_2400K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_2560K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_3200K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(1536000));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(1200000));

    /* Window edges (low min is 225001 — not 225000; ratio field / desktop parity) */
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_LOW_MIN_HZ));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_LOW_MAX_HZ));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_HIGH_MIN_HZ));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_HIGH_MAX_HZ));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(225001));

    /* Rejects */
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(0));
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(225000)); /* desktop + ratio mask */
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_LOW_MIN_HZ - 1));
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_LOW_MAX_HZ + 1)); /* gap */
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(500000));
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_HIGH_MIN_HZ - 1));
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_HIGH_MAX_HZ + 1));
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(4000000));

    uint32_t exact = 0;
    EXPECT_TRUE(esp_rtl_sdr_quantize_sample_rate(2048000, &exact));
    EXPECT_TRUE(exact > 0);
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(exact));

    /* Idempotent: quantize(exact) stays exact (or very close re-quantizable) */
    uint32_t exact2 = 0;
    EXPECT_TRUE(esp_rtl_sdr_quantize_sample_rate(exact, &exact2));
    EXPECT_EQ_U(exact2, exact);

    /* Re-quantize request maps to same exact */
    uint32_t again = 0;
    EXPECT_TRUE(esp_rtl_sdr_quantize_sample_rate(2048000, &again));
    EXPECT_EQ_U(again, exact);

    EXPECT_TRUE(!esp_rtl_sdr_quantize_sample_rate(500000, &exact));
    EXPECT_TRUE(!esp_rtl_sdr_quantize_sample_rate(2048000, nullptr));
    EXPECT_TRUE(!esp_rtl_sdr_quantize_sample_rate(0, &exact));

    /* XTAL constant used by formula */
    EXPECT_EQ_U(ESP_RTL_SDR_XTAL_HZ, 28800000u);
}

static void test_recommended_rates(void)
{
    size_t n = 0;
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(nullptr, 0, &n), ESP_OK);
    EXPECT_TRUE(n >= 8);
    EXPECT_TRUE(n <= 16);

    uint32_t rates[16];
    size_t written = 0;
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(rates, n, &written), ESP_OK);
    EXPECT_EQ_U(written, n);

    bool has_960 = false, has_2048 = false, has_2560 = false;
    uint32_t prev = 0;
    for (size_t i = 0; i < written; ++i) {
        EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(rates[i]));
        if (i > 0) {
            EXPECT_TRUE(rates[i] >= prev); /* non-decreasing list */
        }
        prev = rates[i];
        if (rates[i] == ESP_RTL_SDR_RATE_960K) {
            has_960 = true;
        }
        if (rates[i] == ESP_RTL_SDR_RATE_2048K) {
            has_2048 = true;
        }
        if (rates[i] == ESP_RTL_SDR_RATE_2560K) {
            has_2560 = true;
        }
    }
    EXPECT_TRUE(has_960);
    EXPECT_TRUE(has_2048);
    EXPECT_TRUE(has_2560);

    /* Truncated buffer: INVALID_SIZE but still reports total */
    size_t total = 0;
    uint32_t one = 0;
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(&one, 1, &total), ESP_ERR_INVALID_SIZE);
    EXPECT_EQ_U(total, n);
    EXPECT_EQ_U(one, rates[0]);

    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(nullptr, 1, &n), ESP_ERR_INVALID_ARG);
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(rates, 1, nullptr), ESP_ERR_INVALID_ARG);
}

static void test_frequency(void)
{
    uint32_t q = 0;
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(96123456, &q));
    EXPECT_EQ_U(q, 96123000);

    /* Already quantized */
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(100000000, &q));
    EXPECT_EQ_U(q, 100000000);

    /* Range edges — full V4 span incl. HF (0.7.7) */
    EXPECT_EQ_U(ESP_RTL_SDR_FREQ_MIN_HZ, 500000u);
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(ESP_RTL_SDR_FREQ_MIN_HZ, &q));
    EXPECT_EQ_U(q, ESP_RTL_SDR_FREQ_MIN_HZ);
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(ESP_RTL_SDR_FREQ_MAX_HZ, &q));
    EXPECT_EQ_U(q, ESP_RTL_SDR_FREQ_MAX_HZ);
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(5000000u, &q));
    EXPECT_EQ_U(q, 5000000u);
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(15105000u, &q));
    EXPECT_EQ_U(q, 15105000u);

    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(ESP_RTL_SDR_FREQ_MIN_HZ - 1, &q));
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(ESP_RTL_SDR_FREQ_MAX_HZ + 1, &q));
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(1000, &q));
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(2000000000u, &q));
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(100000000, nullptr));

    /* HF upconverter map (public +28.8 MHz) */
    EXPECT_TRUE(esp_rtl_sdr_frequency_uses_hf_upconverter(10000000u));
    EXPECT_TRUE(esp_rtl_sdr_frequency_uses_hf_upconverter(28799999u));
    EXPECT_TRUE(!esp_rtl_sdr_frequency_uses_hf_upconverter(28800000u));
    EXPECT_TRUE(!esp_rtl_sdr_frequency_uses_hf_upconverter(100000000u));
    EXPECT_EQ_U(esp_rtl_sdr_tuner_frequency_hz(10000000u), 38800000u);
    EXPECT_EQ_U(esp_rtl_sdr_tuner_frequency_hz(5000000u), 33800000u);
    EXPECT_EQ_U(esp_rtl_sdr_tuner_frequency_hz(100000000u), 100000000u);

    /* Quant step */
    EXPECT_EQ_U(ESP_RTL_SDR_FREQ_QUANT_HZ, 1000u);
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(100000999, &q));
    EXPECT_EQ_U(q, 100000000);

    uint32_t preset = 0;
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_KZEL_96_1, &preset),
                ESP_OK);
    EXPECT_EQ_U(preset, ESP_RTL_SDR_PRESET_KZEL_HZ);
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_NOAA_162_4, &preset),
                ESP_OK);
    EXPECT_EQ_U(preset, ESP_RTL_SDR_PRESET_NOAA_HZ);
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_CUSTOM_HZ, &preset),
                ESP_ERR_INVALID_ARG);
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_KZEL_96_1, nullptr),
                ESP_ERR_INVALID_ARG);
}

static void test_config_validate(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    EXPECT_EQ_U(cfg.struct_size, sizeof(cfg));
    EXPECT_EQ_U(cfg.transfer_bytes, ESP_RTL_SDR_DEFAULT_XFER_BYTES);
    EXPECT_EQ_U(cfg.transfer_count, ESP_RTL_SDR_DEFAULT_XFER_COUNT);
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);

    cfg.struct_size = 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    /* Append-only ABI: exact sizeof still OK (min..sizeof). */
    esp_rtl_sdr_config_default(&cfg);
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.struct_size = sizeof(cfg) + 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_bytes = 1000; /* not multiple of 512 */
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_bytes = 511;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_bytes = ESP_RTL_SDR_MIN_XFER_BYTES;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);

    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_count = 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_count = ESP_RTL_SDR_MAX_XFER_COUNT;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);

    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_count = ESP_RTL_SDR_MAX_XFER_COUNT + 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.control_timeout_ms = 0;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.control_timeout_ms = ESP_RTL_SDR_MAX_TIMEOUT_MS + 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.usb_task_core_id = 2;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.usb_task_core_id = 0;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.usb_task_core_id = 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.usb_task_core_id = 0xFF;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);

    esp_rtl_sdr_config_default(&cfg);
    EXPECT_EQ_U((unsigned)cfg.delivery_mode, (unsigned)ESP_RTL_SDR_DELIVERY_BOTH);
    EXPECT_EQ_U(cfg.pull_ring_bytes, 0);
    cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_CALLBACK;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_READ;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.delivery_mode = static_cast<esp_rtl_sdr_delivery_mode_t>(99);
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_config_default(&cfg);
    cfg.pull_ring_bytes = 4096;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.pull_ring_bytes = 1001; /* odd */
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);
    cfg.pull_ring_bytes = 512; /* too small */
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);

    /* Old struct_size: trailing delivery fields default to BOTH / 0 */
    esp_rtl_sdr_config_default(&cfg);
    cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_READ;
    cfg.pull_ring_bytes = 8192;
    {
        esp_rtl_sdr_config_t old = cfg;
        old.struct_size =
            offsetof(esp_rtl_sdr_config_t, usb_task_core_id) + sizeof(uint8_t);
        EXPECT_EQ_I(esp_rtl_sdr_config_validate(&old), ESP_OK);
    }

    EXPECT_EQ_I(esp_rtl_sdr_config_validate(nullptr), ESP_ERR_INVALID_ARG);
    esp_rtl_sdr_config_default(nullptr); /* no crash */

    esp_rtl_sdr_stream_config_t st;
    esp_rtl_sdr_stream_config_default(&st);
    EXPECT_EQ_U(st.struct_size, sizeof(st));
    EXPECT_EQ_U(st.sample_rate_sps, ESP_RTL_SDR_RATE_960K);
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_OK);

    st.sample_rate_sps = 12345;
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_RTL_SDR_ERR_BAD_RATE);

    st.sample_rate_sps = 0; /* allowed — filled at start */
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_OK);

    st.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
    st.frequency_hz = 1000;
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_RTL_SDR_ERR_BAD_FREQ);

    st.frequency_hz = 100000000;
    st.sample_rate_sps = ESP_RTL_SDR_RATE_2048K;
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_OK);

    st.max_bytes = 3; /* odd */
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_ERR_INVALID_ARG);

    st.max_bytes = 2;
    st.timeout_ms = ESP_RTL_SDR_MAX_TIMEOUT_MS + 1;
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_ERR_INVALID_ARG);

    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(nullptr), ESP_ERR_INVALID_ARG);
    esp_rtl_sdr_stream_config_default(nullptr);
}

static void test_names(void)
{
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_UNINSTALLED), "UNINSTALLED");
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_IDLE), "IDLE");
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_STREAMING), "STREAMING");
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_STOPPING), "STOPPING");
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_FAULT), "FAULT");
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_STARTING), "STARTING");
    EXPECT_STREQ(esp_rtl_sdr_state_to_name(static_cast<esp_rtl_sdr_state_t>(99)), "UNKNOWN");

    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_OK), "ESP_OK");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_ERR_INVALID_ARG), "ESP_ERR_INVALID_ARG");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_NO_DEVICE), "ESP_RTL_SDR_ERR_NO_DEVICE");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_BAD_RATE), "ESP_RTL_SDR_ERR_BAD_RATE");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_BAD_FREQ), "ESP_RTL_SDR_ERR_BAD_FREQ");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_BUSY), "ESP_RTL_SDR_ERR_BUSY");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_UNSUPPORTED),
                 "ESP_RTL_SDR_ERR_UNSUPPORTED");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_BAD_DEVICE),
                 "ESP_RTL_SDR_ERR_BAD_DEVICE");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_NOT_V4),
                 "ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE");
    EXPECT_STREQ(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE),
                 "ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE");

    /* Never null */
    EXPECT_TRUE(esp_rtl_sdr_err_to_name(static_cast<esp_err_t>(-99999)) != nullptr);
}

static void test_passport_opts(void)
{
    esp_rtl_sdr_passport_opts_t o;
    esp_rtl_sdr_passport_opts_default(&o);
    EXPECT_EQ_U(o.struct_size, sizeof(o));
    EXPECT_EQ_U(o.dwell_ms, ESP_RTL_SDR_PASSPORT_DEFAULT_DWELL_MS);
    EXPECT_EQ_U(o.min_efficiency_pct, 95u);
    EXPECT_TRUE(o.recommended_only);
    EXPECT_EQ_U(o.frequency_hz, 0u);
    EXPECT_TRUE(ESP_RTL_SDR_PASSPORT_MAX_ENTRIES >= 8);
    esp_rtl_sdr_passport_opts_default(nullptr); /* no crash */
}

static void test_delivery_mode_helpers(void)
{
    EXPECT_TRUE(esp_rtl_sdr_delivery_mode_uses_callback_iq(ESP_RTL_SDR_DELIVERY_BOTH));
    EXPECT_TRUE(esp_rtl_sdr_delivery_mode_uses_read(ESP_RTL_SDR_DELIVERY_BOTH));
    EXPECT_TRUE(esp_rtl_sdr_delivery_mode_uses_callback_iq(ESP_RTL_SDR_DELIVERY_CALLBACK));
    EXPECT_TRUE(!esp_rtl_sdr_delivery_mode_uses_read(ESP_RTL_SDR_DELIVERY_CALLBACK));
    EXPECT_TRUE(!esp_rtl_sdr_delivery_mode_uses_callback_iq(ESP_RTL_SDR_DELIVERY_READ));
    EXPECT_TRUE(esp_rtl_sdr_delivery_mode_uses_read(ESP_RTL_SDR_DELIVERY_READ));
}

static void test_usb_identity_constants(void)
{
    EXPECT_EQ_U(ESP_RTL_SDR_USB_VID, 0x0BDAu);
    EXPECT_EQ_U(ESP_RTL_SDR_USB_PID, 0x2838u);
    EXPECT_TRUE(ESP_RTL_SDR_PPM_MIN < 0);
    EXPECT_EQ_I(ESP_RTL_SDR_PPM_MIN, -200);
    EXPECT_EQ_I(ESP_RTL_SDR_PPM_MAX, 200);
    EXPECT_EQ_U(ESP_RTL_SDR_DEFAULT_XFER_BYTES % 512u, 0u);
    EXPECT_TRUE(ESP_RTL_SDR_MIN_XFER_COUNT >= 2);
    EXPECT_TRUE(ESP_RTL_SDR_MAX_XFER_COUNT >= ESP_RTL_SDR_MIN_XFER_COUNT);
}

static void test_error_base_unique(void)
{
    /* Distinct component codes (spot-check) */
    EXPECT_TRUE(ESP_RTL_SDR_ERR_NO_DEVICE != ESP_RTL_SDR_ERR_BUSY);
    EXPECT_TRUE(ESP_RTL_SDR_ERR_BAD_RATE != ESP_RTL_SDR_ERR_BAD_FREQ);
    EXPECT_TRUE(ESP_RTL_SDR_ERR_UNSUPPORTED != ESP_RTL_SDR_ERR_STALE_HANDLE);
    EXPECT_EQ_I(ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE, ESP_RTL_SDR_ERR_NOT_V4);
}

int main(void)
{
    std::printf("esp_rtl_sdr host policy tests (%s)\n", esp_rtl_sdr_get_version_string());
    test_version();
    test_capabilities();
    test_measured_agc_tables();
    test_callback_reentry_is_task_local();
    test_delivery_mode_helpers();
    test_rate_windows();
    test_recommended_rates();
    test_frequency();
    test_config_validate();
    test_names();
    test_passport_opts();
    test_usb_identity_constants();
    test_error_base_unique();
    std::printf("RESULT passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
