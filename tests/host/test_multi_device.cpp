/*
 * Host unit tests for multi-receiver primitives (no USB / FreeRTOS / IDF).
 * Hardware concurrent-stream tests live in examples/multi_rtlsdr_test/.
 */

#include <cstdio>
#include <cstring>

#include "esp_rtl_sdr.h"
#include "rtl_multi.hpp"

static int g_failed = 0;
static int g_passed = 0;

#define EXPECT_TRUE(cond)                                                                          \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                            \
            g_failed++;                                                                            \
        } else {                                                                                   \
            g_passed++;                                                                            \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ_U(a, b)                                                                          \
    do {                                                                                           \
        const auto _a = (a);                                                                       \
        const auto _b = (b);                                                                       \
        if (_a != _b) {                                                                            \
            std::printf("FAIL %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, #a,                \
                        (unsigned)_a, #b, (unsigned)_b);                                           \
            g_failed++;                                                                            \
        } else {                                                                                   \
            g_passed++;                                                                            \
        }                                                                                          \
    } while (0)

#define EXPECT_EQ_I(a, b)                                                                          \
    do {                                                                                           \
        const auto _a = (a);                                                                       \
        const auto _b = (b);                                                                       \
        if (_a != _b) {                                                                            \
            std::printf("FAIL %s:%d: %s (%d) != %s (%d)\n", __FILE__, __LINE__, #a, (int)_a, #b,   \
                        (int)_b);                                                                  \
            g_failed++;                                                                            \
        } else {                                                                                   \
            g_passed++;                                                                            \
        }                                                                                          \
    } while (0)

#define EXPECT_STREQ(a, b)                                                                         \
    do {                                                                                           \
        const char *_a = (a);                                                                      \
        const char *_b = (b);                                                                      \
        if (_a == nullptr || _b == nullptr || std::strcmp(_a, _b) != 0) {                          \
            std::printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,                      \
                        _a ? _a : "(null)", _b ? _b : "(null)");                                   \
            g_failed++;                                                                            \
        } else {                                                                                   \
            g_passed++;                                                                            \
        }                                                                                          \
    } while (0)

static void test_logical_index_alloc(void)
{
    bool used[ESP_RTL_SDR_MAX_DEVICES]{};
    const int a = rtl_logical_alloc(used);
    const int b = rtl_logical_alloc(used);
    const int c = rtl_logical_alloc(used);
    EXPECT_EQ_I(a, 0);
    EXPECT_EQ_I(b, 1);
    EXPECT_EQ_I(c, 2);
    EXPECT_TRUE(rtl_logical_index_valid(a));
    EXPECT_TRUE(!rtl_logical_index_valid(-1));
    EXPECT_TRUE(!rtl_logical_index_valid(ESP_RTL_SDR_MAX_DEVICES));
    rtl_logical_free(used, b);
    const int b2 = rtl_logical_alloc(used);
    EXPECT_EQ_I(b2, 1);
    EXPECT_TRUE(rtl_logical_alloc(nullptr) < 0);
}

static void test_claim_exclusive(void)
{
    RtlClaimTable t{};
    rtl_claim_clear(&t);
    EXPECT_TRUE(rtl_claim_try(&t, 3, 0));
    EXPECT_TRUE(rtl_claim_try(&t, 3, 0)); /* same owner re-claim OK */
    EXPECT_TRUE(!rtl_claim_try(&t, 3, 1)); /* other owner refused */
    EXPECT_TRUE(rtl_claim_taken_by_other(&t, 3, 1));
    EXPECT_TRUE(!rtl_claim_taken_by_other(&t, 3, 0));
    EXPECT_TRUE(rtl_claim_try(&t, 4, 1));
    EXPECT_EQ_U(rtl_claim_count(&t), 2u);
    EXPECT_TRUE(rtl_claim_no_duplicate_addrs(&t));

    rtl_claim_release(&t, 3, 0);
    EXPECT_TRUE(!rtl_claim_taken_by_other(&t, 3, 1));
    EXPECT_TRUE(rtl_claim_try(&t, 3, 1)); /* now free for owner 1 */
    EXPECT_EQ_U(rtl_claim_count(&t), 2u);

    rtl_claim_release_owner(&t, 1);
    EXPECT_EQ_U(rtl_claim_count(&t), 0u);

    EXPECT_TRUE(!rtl_claim_try(&t, 0, 0)); /* addr 0 invalid */
    EXPECT_TRUE(!rtl_claim_try(nullptr, 5, 0));
    EXPECT_TRUE(!rtl_claim_try(&t, 5, ESP_RTL_SDR_MAX_DEVICES));
}

static void test_claim_cleanup_on_disconnect(void)
{
    RtlClaimTable t{};
    EXPECT_TRUE(rtl_claim_try(&t, 5, 0));
    EXPECT_TRUE(rtl_claim_try(&t, 6, 1));
    EXPECT_TRUE(rtl_claim_try(&t, 7, 2));
    /* One receiver disconnects — others keep their claims. */
    rtl_claim_release(&t, 6, 1);
    EXPECT_TRUE(rtl_claim_taken_by_other(&t, 5, 1));
    EXPECT_TRUE(!rtl_claim_taken_by_other(&t, 6, 0));
    EXPECT_TRUE(rtl_claim_taken_by_other(&t, 7, 1));
    EXPECT_EQ_U(rtl_claim_count(&t), 2u);
}

static void test_sequence_numbers(void)
{
    uint32_t seq = 0;
    EXPECT_EQ_U(rtl_next_sequence(&seq), 1u);
    EXPECT_EQ_U(rtl_next_sequence(&seq), 2u);
    EXPECT_EQ_U(seq, 2u);
    EXPECT_EQ_U(rtl_next_sequence(nullptr), 0u);
}

static void test_config_isolation(void)
{
    RtlReceiverConfig a{};
    RtlReceiverConfig b{};
    a.frequency_hz = 99100000;
    a.sample_rate_sps = 2400000;
    a.gain_tenth_db = 280;
    b.frequency_hz = 101100000;
    b.sample_rate_sps = 2400000;
    b.gain_tenth_db = 240;
    EXPECT_TRUE(!rtl_receiver_config_equal(a, b));
    /* Mutating B must not touch A (distinct objects). */
    b.frequency_hz = 103100000;
    b.gain_tenth_db = 320;
    EXPECT_EQ_U(a.frequency_hz, 99100000u);
    EXPECT_EQ_I(a.gain_tenth_db, 280);
    EXPECT_EQ_U(b.frequency_hz, 103100000u);
    EXPECT_TRUE(&a != &b);
}

static void test_usb_path_and_identity(void)
{
    char path[24];
    esp_rtl_sdr_format_usb_path(0, 0, path, sizeof(path));
    EXPECT_STREQ(path, "root");
    esp_rtl_sdr_format_usb_path(1, 2, path, sizeof(path));
    EXPECT_STREQ(path, "P1.p2");
    esp_rtl_sdr_format_usb_path(4, 3, nullptr, 8);

    esp_rtl_sdr_identity_t id;
    esp_rtl_sdr_identity_default(&id);
    EXPECT_EQ_U(id.struct_size, sizeof(id));
    EXPECT_EQ_U(id.usb_addr, 0u);
    EXPECT_STREQ(id.usb_path, "root");
    esp_rtl_sdr_identity_default(nullptr);
}

static void test_capture_metadata(void)
{
    esp_rtl_sdr_capture_meta_t m;
    esp_rtl_sdr_fill_capture_meta(&m, 1, 4, 2, 42, 123456, 8192, 101100000, 101100000, 2400000,
                                  ESP_RTL_SDR_GAIN_MODE_MANUAL, 240, 0,
                                  ESP_RTL_SDR_PROFILE_BLOG_V4, false, ESP_RTL_SDR_STATE_STREAMING,
                                  3, 1, ESP_RTL_SDR_IQ_FLAG_SHORT_TRANSFER);
    EXPECT_EQ_U(m.device_id, 1u);
    EXPECT_EQ_U(m.usb_addr, 4u);
    EXPECT_EQ_U(m.hub_port, 2u);
    EXPECT_EQ_U(m.sequence, 42u);
    EXPECT_TRUE(m.host_timestamp_us == 123456);
    EXPECT_EQ_U(m.sample_count, 8192u);
    EXPECT_EQ_U(m.center_frequency_hz, 101100000u);
    EXPECT_EQ_U(m.bandwidth_hz, 2400000u);
    EXPECT_EQ_I(m.gain_tenth_db, 240);
    EXPECT_EQ_U(m.dropped_buffers, 3u);
    EXPECT_EQ_U(m.usb_errors, 1u);
    EXPECT_EQ_U(m.rms_placeholder, 0u);
    EXPECT_EQ_U(m.peak_placeholder, 0u);
    EXPECT_TRUE((m.flags & ESP_RTL_SDR_IQ_FLAG_SHORT_TRANSFER) != 0);

    esp_rtl_sdr_capture_meta_t other;
    esp_rtl_sdr_fill_capture_meta(&other, 0, 3, 1, 1, 99, 512, 99100000, 99100000, 2400000, 0, 280,
                                  0, ESP_RTL_SDR_PROFILE_BLOG_V4, false, ESP_RTL_SDR_STATE_STREAMING,
                                  0, 0, 0);
    EXPECT_EQ_U(other.device_id, 0u);
    EXPECT_EQ_U(other.center_frequency_hz, 99100000u);
    EXPECT_EQ_U(m.center_frequency_hz, 101100000u); /* isolation */
    esp_rtl_sdr_fill_capture_meta(nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  ESP_RTL_SDR_PROFILE_UNKNOWN, false, ESP_RTL_SDR_STATE_IDLE, 0, 0,
                                  0);
}

static void test_stream_stats_default(void)
{
    esp_rtl_sdr_stream_stats_t s;
    esp_rtl_sdr_stream_stats_default(&s);
    EXPECT_EQ_U(s.struct_size, sizeof(s));
    EXPECT_EQ_U(s.usb_timeouts, 0u);
    EXPECT_EQ_U(s.bytes_received, 0u);
    esp_rtl_sdr_hub_stats_t h;
    esp_rtl_sdr_hub_stats_default(&h);
    EXPECT_EQ_U(h.session_refcount, 0u);
    EXPECT_TRUE(!h.hubs_compiled_in); /* host tests have no sdkconfig hub flag */
}

static void test_bind_config_validate(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    EXPECT_TRUE(cfg.bind_device_index == ESP_RTL_SDR_BIND_ANY);
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.bind_device_index = 2;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_OK);
    cfg.bind_device_index = ESP_RTL_SDR_MAX_DEVICES;
    EXPECT_EQ_I(esp_rtl_sdr_config_validate(&cfg), ESP_RTL_SDR_ERR_BAD_DEVICE);
}

static void test_invalid_device_ids(void)
{
    EXPECT_TRUE(!rtl_logical_index_valid(-1));
    EXPECT_TRUE(!rtl_logical_index_valid(99));
    EXPECT_TRUE(rtl_logical_index_valid(0));
    EXPECT_TRUE(rtl_logical_index_valid(ESP_RTL_SDR_MAX_DEVICES - 1));
}

static void test_iq_block_append_fields_exist(void)
{
    esp_rtl_sdr_iq_block_t b{};
    b.device_id = 2;
    b.gain_tenth_db = 320;
    b.bandwidth_hz = 2400000;
    b.flags = ESP_RTL_SDR_IQ_FLAG_OVERRUN;
    b.host_timestamp_us = 1;
    EXPECT_EQ_U(b.device_id, 2u);
    EXPECT_TRUE(b.host_timestamp_us == 1);
    EXPECT_TRUE(sizeof(esp_rtl_sdr_iq_block_t) > 32u);
}

int main(void)
{
    test_logical_index_alloc();
    test_claim_exclusive();
    test_claim_cleanup_on_disconnect();
    test_sequence_numbers();
    test_config_isolation();
    test_usb_path_and_identity();
    test_capture_metadata();
    test_stream_stats_default();
    test_bind_config_validate();
    test_invalid_device_ids();
    test_iq_block_append_fields_exist();

    std::printf("multi_device tests: passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
