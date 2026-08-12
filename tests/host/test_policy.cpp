/*
 * Host unit tests for esp_rtl_sdr pure policy (no USB / FreeRTOS / IDF).
 * TheOrc-style: claims about rates/config/version must have automated proof.
 *
 * Build & run: tests/scripts/run_host_tests.ps1  (or .sh)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_rtl_sdr.h"

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

static void test_version(void)
{
    EXPECT_EQ_U(esp_rtl_sdr_get_version_string()[0], '0');
    const uint32_t packed = esp_rtl_sdr_get_version();
    EXPECT_EQ_U((packed >> 16) & 0xff, ESP_RTL_SDR_VERSION_MAJOR);
    EXPECT_EQ_U((packed >> 8) & 0xff, ESP_RTL_SDR_VERSION_MINOR);
    EXPECT_EQ_U(packed & 0xff, ESP_RTL_SDR_VERSION_PATCH);
}

static void test_capabilities(void)
{
    const uint32_t c = esp_rtl_sdr_get_capabilities();
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_CONTINUOUS_RATE) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_NEED) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_HEALTH) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_PASSPORT) != 0);
    EXPECT_TRUE((c & ESP_RTL_SDR_CAP_BIAS_TEE) == 0); /* not implemented */
}

static void test_rate_windows(void)
{
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_250K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_960K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_2048K));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(1536000));
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_3200K));

    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(0));
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(500000));  /* gap */
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(100000));  /* below low */
    EXPECT_TRUE(!esp_rtl_sdr_is_rate_supported(4000000)); /* above max */

    uint32_t exact = 0;
    EXPECT_TRUE(esp_rtl_sdr_quantize_sample_rate(2048000, &exact));
    EXPECT_TRUE(exact > 0);
    EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(exact));

    EXPECT_TRUE(!esp_rtl_sdr_quantize_sample_rate(500000, &exact));
    EXPECT_TRUE(!esp_rtl_sdr_quantize_sample_rate(2048000, nullptr));
}

static void test_recommended_rates(void)
{
    size_t n = 0;
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(nullptr, 0, &n), ESP_OK);
    EXPECT_TRUE(n >= 8);

    uint32_t rates[16];
    size_t written = 0;
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(rates, n, &written), ESP_OK);
    EXPECT_EQ_U(written, n);
    bool has_960 = false, has_2048 = false;
    for (size_t i = 0; i < written; ++i) {
        EXPECT_TRUE(esp_rtl_sdr_is_rate_supported(rates[i]));
        if (rates[i] == ESP_RTL_SDR_RATE_960K) {
            has_960 = true;
        }
        if (rates[i] == ESP_RTL_SDR_RATE_2048K) {
            has_2048 = true;
        }
    }
    EXPECT_TRUE(has_960);
    EXPECT_TRUE(has_2048);

    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(nullptr, 1, &n), ESP_ERR_INVALID_ARG);
    EXPECT_EQ_I(esp_rtl_sdr_get_supported_rates(rates, 1, nullptr), ESP_ERR_INVALID_ARG);
}

static void test_frequency(void)
{
    uint32_t q = 0;
    EXPECT_TRUE(esp_rtl_sdr_normalize_frequency(96123456, &q));
    EXPECT_EQ_U(q, 96123000);
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(1000, &q));
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(2000000000u, &q));
    EXPECT_TRUE(!esp_rtl_sdr_normalize_frequency(100000000, nullptr));

    uint32_t preset = 0;
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_KZEL_96_1, &preset),
                ESP_OK);
    EXPECT_EQ_U(preset, ESP_RTL_SDR_PRESET_KZEL_HZ);
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_NOAA_162_4, &preset),
                ESP_OK);
    EXPECT_EQ_U(preset, ESP_RTL_SDR_PRESET_NOAA_HZ);
    EXPECT_EQ_I(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_CUSTOM_HZ, &preset),
                ESP_ERR_INVALID_ARG);
}

static void test_config_validate(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.struct_size = 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);
    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_bytes = 1000; /* not multiple of 512 */
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);
    esp_rtl_sdr_config_default(&cfg);
    cfg.transfer_count = 1;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_ERR_INVALID_ARG);
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(nullptr), ESP_ERR_INVALID_ARG);

    esp_rtl_sdr_stream_config_t st;
    esp_rtl_sdr_stream_config_default(&st);
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_OK);
    st.sample_rate_sps = 12345;
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_RTL_SDR_ERR_BAD_RATE);
    st.sample_rate_sps = 0; /* allowed — filled at start */
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_OK);
    st.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
    st.frequency_hz = 1000;
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_RTL_SDR_ERR_BAD_FREQ);
    st.frequency_hz = 100000000;
    st.max_bytes = 3; /* odd */
    EXPECT_EQ_I(esp_rtl_sdr_stream_config_validate(&st), ESP_ERR_INVALID_ARG);
}

static void test_names(void)
{
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_state_to_name(ESP_RTL_SDR_STATE_IDLE), "IDLE") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_err_to_name(ESP_OK), "ESP_OK") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_BAD_RATE),
                            "ESP_RTL_SDR_ERR_BAD_RATE") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_err_to_name(ESP_RTL_SDR_ERR_NOT_V4),
                            "ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE") == 0);
}

static void test_passport_opts(void)
{
    esp_rtl_sdr_passport_opts_t o;
    esp_rtl_sdr_passport_opts_default(&o);
    EXPECT_EQ_U(o.struct_size, sizeof(o));
    EXPECT_EQ_U(o.dwell_ms, ESP_RTL_SDR_PASSPORT_DEFAULT_DWELL_MS);
    EXPECT_EQ_U(o.min_efficiency_pct, 95u);
    EXPECT_TRUE(o.recommended_only);
    esp_rtl_sdr_passport_opts_default(nullptr); /* no crash */
}

int main(void)
{
    std::printf("esp_rtl_sdr host policy tests (%s)\n", esp_rtl_sdr_get_version_string());
    test_version();
    test_capabilities();
    test_rate_windows();
    test_recommended_rates();
    test_frequency();
    test_config_validate();
    test_names();
    test_passport_opts();
    std::printf("RESULT passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
