/*
 * Behavioral host tests for multi-dongle identity / caps / frequency policy.
 * No USB / FreeRTOS. Pure profile + V4 frontend composition + tuner isolation.
 */
#include "esp_rtl_sdr.h"
#include "gain_r820t2.hpp"
#include "measured_gain_bias_v4.hpp"
#include "rtl_profile.hpp"
#include "transfers_blog_v3.hpp"
#include "transfers_blog_v4.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>

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

static void test_detection_matrix(void)
{
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V4"),
                (unsigned)RtlProfileId::BlogV4);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V3"),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog",
                                                       "RTL-SDR Blog V3"),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "Nooelec",
                                                       "NESDR SMArt v5"),
                (unsigned)RtlProfileId::NooelecSmartV5);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "Nooelec",
                                                       "NESDR SMArt v5 Bulk"),
                (unsigned)RtlProfileId::NooelecSmartV5);

    /* Never treat bare / unknown 0bda:2838 as V4. */
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTL2832U", "Generic"),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_from_descriptors(0x0BDA, 0x2838, "RTLSDRBlog", "unknown"),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic", {}),
                (unsigned)RtlProfileId::Unknown);
}

static void test_unknown_reject_and_v3_probe(void)
{
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {true, 0x96}),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {true, 0x69}),
                (unsigned)RtlProfileId::BlogV3);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {false, 0x96}),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "Generic",
                                             {true, 0x00}),
                (unsigned)RtlProfileId::Unknown);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x1234, 0x2838, "RTLSDRBlog", "Blog V4", {}),
                (unsigned)RtlProfileId::Unknown);

    /* Descriptor always wins over probe. */
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V4",
                                             {true, 0x96}),
                (unsigned)RtlProfileId::BlogV4);
    EXPECT_EQ_U((unsigned)rtl_profile_select(0x0BDA, 0x2838, "Nooelec", "NESDR SMArt v5",
                                             {true, 0x96}),
                (unsigned)RtlProfileId::NooelecSmartV5);

    EXPECT_EQ_U(kBlogV3ProbeSelect.value, 0x0034);
    EXPECT_EQ_U(kBlogV3ProbeRead.value, 0x0034);
    EXPECT_EQ_U(kBlogV3ProbeSelect.index, 0x0610);
    EXPECT_EQ_U(kBlogV3ProbeRead.index, 0x0600);
    EXPECT_EQ_U(kBlogV3ProbeSelect.request_type, 0x40);
    EXPECT_EQ_U(kBlogV3ProbeRead.request_type, 0xc0);
}

static void test_tuner_isolation(void)
{
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::BlogV4), 0x0074);
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::BlogV3), 0x0034);
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::NooelecSmartV5), 0x0034);
    EXPECT_EQ_U(rtl_profile_tuner_i2c_value(RtlProfileId::Unknown), 0);

    /* Remap policy: only low byte of tuner IR value when index is 0x0610/0x0600. */
    RtlControlRecord v4_ir = {0x0074, 0x0610, 0x40, 2, {0x05, 0xa3}};
    EXPECT_EQ_U(v4_ir.value & 0xff, kBlogV4TunerI2cValue);
    EXPECT_TRUE(kRtlFinalTuneTemplate[0].value == 0x0074);

    size_t v4_allowed = 0;
    size_t v3_allowed = 0;
    size_t nooelec_allowed = 0;
    for (const auto &record : kRtlInitTransfers) {
        v4_allowed += rtl_profile_allows_init_record(RtlProfileId::BlogV4, record) ? 1 : 0;
        v3_allowed += rtl_profile_allows_init_record(RtlProfileId::BlogV3, record) ? 1 : 0;
        nooelec_allowed +=
            rtl_profile_allows_init_record(RtlProfileId::NooelecSmartV5, record) ? 1 : 0;
        if (record.value == 0x3001 || record.value == 0x3003 || record.value == 0x3004) {
            EXPECT_TRUE(rtl_profile_allows_init_record(RtlProfileId::BlogV4, record));
            EXPECT_TRUE(!rtl_profile_allows_init_record(RtlProfileId::BlogV3, record));
            EXPECT_TRUE(!rtl_profile_allows_init_record(RtlProfileId::NooelecSmartV5, record));
        }
    }
    EXPECT_EQ_U(v4_allowed, std::size(kRtlInitTransfers));
    EXPECT_TRUE(v3_allowed < v4_allowed);
    EXPECT_EQ_U(v3_allowed, nooelec_allowed);
}

static void test_frequency_policy(void)
{
    /* V4: experimental LF/HF upconverter path, exact requested RF retained. */
    const uint32_t v4_rf_hz[] = {24000u, 60000u, 135600u, 147300u, 474000u,
                                 500000u, 1000000u, 10000000u, 28799999u,
                                 28800000u, 28800001u, 28801000u, 96100000u, 162400000u,
                                 433000000u, 1090000000u};
    for (const uint32_t rf_hz : v4_rf_hz) {
        EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV4, rf_hz));
        const uint32_t expected = rf_hz < 28800000u ? rf_hz + 28800000u : rf_hz;
        EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV4, rf_hz), expected);
    }
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::BlogV4, 23999u));
    EXPECT_TRUE(rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV4));

    /* Nooelec: reject < 24 MHz; no V4 HF LO offset / routing. */
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 10000000u));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 23999999u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 24000000u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 100000000u));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::NooelecSmartV5, 100000000u),
                100000000u);
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::NooelecSmartV5));

    /* V3: captured Q-branch direct sampling below the 24 MHz tuner floor. */
    const uint32_t direct_rf_hz[] = {60000u, 135600u, 147300u, 474000u, 500000u,
                                     1000000u, 10000000u, 20000000u, 23999999u};
    for (const uint32_t rf_hz : direct_rf_hz) {
        EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, rf_hz));
        EXPECT_TRUE(rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV3, rf_hz));
        EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, rf_hz), 0u);
    }
    const uint32_t normal_v3_rf_hz[] = {24000000u, 28800000u, 96100000u,
                                        162400000u, 433000000u, 1090000000u};
    for (const uint32_t rf_hz : normal_v3_rf_hz) {
        EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, rf_hz));
        EXPECT_TRUE(!rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV3, rf_hz));
        EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, rf_hz), rf_hz);
    }
    EXPECT_TRUE(rtl_profile_supports_stream(RtlProfileId::BlogV3));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, 100000000u),
                100000000u);
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV3));
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV3));
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::NooelecSmartV5));
    EXPECT_TRUE(!rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV4));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::Unknown, 100000000u));

    struct DirectNcoCase { uint32_t rf_hz; uint32_t nco; };
    constexpr DirectNcoCase captured[] = {
        {60000u, 0x3fdddeu}, {135600u, 0x3fb2dcu}, {147300u, 0x3fac34u},
        {472500u, 0x3ef334u}, {1000123u, 0x3dc70bu}, {10000000u, 0x29c71du},
        {20000000u, 0x138e39u}, {23999999u, 0x0aaaabu},
    };
    for (const auto &point : captured) {
        EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(point.rf_hz), point.nco);
    }

    EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(147300u, 100), 0x3fac32u);
    EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(147300u, -100), 0x3fac36u);
    EXPECT_EQ_U(rtl_profile_v3_direct_nco_word(23999999u, 1000), 0x0a9d04u);
}

static void test_matched_if_policy(void)
{
    EXPECT_EQ_U((uint32_t)rtl_profile_pll_if_offset_hz(RtlProfileId::BlogV3), 3570000u);
    EXPECT_EQ_U(rtl_profile_demod_if_restore_hz(RtlProfileId::BlogV3), 3570000u);
    EXPECT_EQ_U((uint32_t)rtl_profile_pll_if_offset_hz(RtlProfileId::BlogV4), 1814972u);
    EXPECT_EQ_U(rtl_profile_demod_if_restore_hz(RtlProfileId::BlogV4), 0u);
    EXPECT_EQ_U((uint32_t)rtl_profile_pll_if_offset_hz(RtlProfileId::NooelecSmartV5), 1814972u);
    EXPECT_EQ_U(rtl_profile_demod_if_restore_hz(RtlProfileId::NooelecSmartV5), 0u);

    const uint16_t values[] = {0x1920, 0x0120, 0x1a20, 0x0120, 0x1b20, 0x0120};
    const uint16_t indices[] = {0x0011, 0x000a, 0x0011, 0x000a, 0x0011, 0x000a};
    const uint8_t request_types[] = {0x40, 0xc0, 0x40, 0xc0, 0x40, 0xc0};
    const uint8_t data[] = {0x38, 0x00, 0x11, 0x00, 0x12, 0x00};
    EXPECT_EQ_U(kRtlStandardIfLast - kRtlStandardIfFirst + 1, 6u);
    for (size_t i = 0; i < 6; ++i) {
        const auto &record = kRtlInitTransfers[kRtlStandardIfFirst + i];
        EXPECT_EQ_U(record.value, values[i]);
        EXPECT_EQ_U(record.index, indices[i]);
        EXPECT_EQ_U(record.request_type, request_types[i]);
        EXPECT_EQ_U(record.length, 1u);
        EXPECT_EQ_U(record.data[0], data[i]);
    }
}

static void test_v3_direct_transition_records(void)
{
    EXPECT_EQ_U(std::size(kBlogV3DirectEnable), 8u);
    EXPECT_EQ_U(kBlogV3DirectEnable[0].value, 0xb120u);
    EXPECT_EQ_U(kBlogV3DirectEnable[0].data[0], 0x1au);
    EXPECT_EQ_U(kBlogV3DirectEnable[2].value, 0x1520u);
    EXPECT_EQ_U(kBlogV3DirectEnable[2].data[0], 0x00u);
    EXPECT_EQ_U(kBlogV3DirectEnable[4].data[0], 0x4du);
    EXPECT_EQ_U(kBlogV3DirectEnable[6].data[0], 0x90u);
    EXPECT_EQ_U(std::size(kBlogV3DirectDisable), 4u);
    EXPECT_EQ_U(kBlogV3DirectDisable[0].data[0], 0x01u);
    EXPECT_EQ_U(kBlogV3DirectDisable[2].data[0], 0x80u);
    EXPECT_EQ_U(kRtlTunerCleanupLast, 13u);
    EXPECT_EQ_U(kRtlTunerReinitFirst, 363u);
    EXPECT_EQ_U(kRtlTunerReinitLast, 419u);
    EXPECT_EQ_U(kRtlInitTransfers[kRtlTunerReinitFirst].value, 0x0074u);
    EXPECT_EQ_U(kRtlInitTransfers[kRtlTunerReinitLast].value, 0x0120u);
    EXPECT_EQ_U(std::size(kBlogV3TunerRepeaterOn), 2u);
    EXPECT_EQ_U(kBlogV3TunerRepeaterOn[0].value, 0x0120u);
    EXPECT_EQ_U(kBlogV3TunerRepeaterOn[0].data[0], 0x18u);

    EXPECT_TRUE(rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV3, 24000000u));
    EXPECT_TRUE(rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV3, 99100000u));
    EXPECT_TRUE(!rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV3, 23999999u));
    EXPECT_TRUE(!rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV4, 99100000u));
    EXPECT_TRUE(!rtl_profile_needs_cold_tuner_reinit(RtlProfileId::NooelecSmartV5,
                                                     99100000u));

    uint8_t full_tail_reg05 = 0;
    uint8_t reinit_reg05 = 0;
    for (size_t i = 0; i < std::size(kRtlInitTransfers); ++i) {
        const auto &record = kRtlInitTransfers[i];
        if (record.value == 0x0074u && record.index == 0x0610u &&
            record.length >= 2 && record.data[0] == 0x05u) {
            full_tail_reg05 = record.data[1];
            if (i >= kRtlTunerReinitFirst && i <= kRtlTunerReinitLast) {
                reinit_reg05 = record.data[1];
            }
        }
    }
    EXPECT_EQ_U(full_tail_reg05, 0xe3u);
    EXPECT_EQ_U(reinit_reg05, 0x83u);

    const uint32_t retunes[] = {96100000u, 10000000u, 147300u, 96100000u, 147300u};
    const bool direct[] = {false, true, true, false, true};
    for (size_t i = 0; i < std::size(retunes); ++i) {
        EXPECT_EQ_U(rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV3, retunes[i]),
                    direct[i]);
    }
}

static void test_capability_matrix(void)
{
    const uint32_t v4 = rtl_profile_device_capabilities(RtlProfileId::BlogV4);
    const uint32_t v3 = rtl_profile_device_capabilities(RtlProfileId::BlogV3);
    const uint32_t noe = rtl_profile_device_capabilities(RtlProfileId::NooelecSmartV5);
    const uint32_t unk = rtl_profile_device_capabilities(RtlProfileId::Unknown);

    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_HF_UPCONVERTER) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_GAIN) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_BIAS_TEE) != 0);
    EXPECT_TRUE((v4 & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) == 0);

    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_RETUNE) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_GAIN) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_GAIN_AUTO) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_BIAS_TEE) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) != 0);
    EXPECT_EQ_U(rtl_profile_default_gain_mode(RtlProfileId::BlogV4),
                ESP_RTL_SDR_GAIN_MODE_AUTO);
    EXPECT_EQ_U(rtl_profile_default_gain_mode(RtlProfileId::BlogV3),
                ESP_RTL_SDR_GAIN_MODE_MANUAL);

    constexpr uint8_t expected_r820t2_stages[][2] = {
        {0x90, 0x60}, {0x91, 0x60}, {0x91, 0x61}, {0x92, 0x61},
        {0x92, 0x62}, {0x93, 0x62}, {0x93, 0x63}, {0x94, 0x63},
        {0x94, 0x64}, {0x95, 0x64}, {0x95, 0x65}, {0x96, 0x65},
        {0x96, 0x66}, {0x97, 0x66}, {0x97, 0x67}, {0x98, 0x67},
        {0x98, 0x68}, {0x99, 0x68}, {0x99, 0x69}, {0x9a, 0x69},
        {0x9a, 0x6a}, {0x9b, 0x6a}, {0x9b, 0x6b}, {0x9c, 0x6b},
        {0x9c, 0x6c}, {0x9d, 0x6c}, {0x9d, 0x6d}, {0x9e, 0x6d},
        {0x9f, 0x6e},
    };
    EXPECT_EQ_U(std::size(kR820T2GainSteps),
                std::size(expected_r820t2_stages));
    for (size_t i = 0; i < std::size(expected_r820t2_stages); ++i) {
        EXPECT_EQ_U(kR820T2GainSteps[i].reg05,
                    expected_r820t2_stages[i][0]);
        EXPECT_EQ_U(kR820T2GainSteps[i].reg07,
                    expected_r820t2_stages[i][1]);
    }
    EXPECT_EQ_U(r820t2_nearest_gain_index(std::numeric_limits<int>::min()), 0u);
    EXPECT_EQ_U(r820t2_nearest_gain_index(0), 0u);
    EXPECT_EQ_U(r820t2_nearest_gain_index(10), 1u);
    EXPECT_EQ_U(r820t2_nearest_gain_index(496),
                std::size(kR820T2GainSteps) - 1);
    EXPECT_EQ_U(r820t2_nearest_gain_index(std::numeric_limits<int>::max()),
                std::size(kR820T2GainSteps) - 1);

    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_RETUNE) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_GAIN) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_BIAS_TEE) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_DIRECT_SAMPLING) == 0);

    EXPECT_EQ_U(unk, 0);
    EXPECT_EQ_U(esp_rtl_sdr_get_capabilities(), rtl_profile_library_capabilities());
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_BLOG_V4),
                             "blog_v4_r828d") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_BLOG_V3),
                             "blog_v3_r820t2") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5),
                             "nooelec_smart_v5_r820t2") == 0);
    EXPECT_TRUE(std::strcmp(esp_rtl_sdr_profile_to_name(ESP_RTL_SDR_PROFILE_UNKNOWN),
                             "unknown") == 0);
}

static void test_profile_transition_matrix(void)
{
    /* Simulated hotplug: attached caps → detach clears to unknown/0. */
    RtlProfileId cur = RtlProfileId::BlogV4;
    uint32_t caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_STREAM) != 0);

    cur = RtlProfileId::Unknown;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_EQ_U(caps, 0);

    cur = RtlProfileId::NooelecSmartV5;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);

    cur = RtlProfileId::Unknown;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_EQ_U(caps, 0);

    cur = RtlProfileId::BlogV3;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);

    cur = RtlProfileId::BlogV4;
    caps = rtl_profile_device_capabilities(cur);
    EXPECT_TRUE((caps & ESP_RTL_SDR_CAP_HF_UPCONVERTER) != 0);

    /* V4 frontend composition must remain identical (PR #18 regression gate). */
    const auto hf = measured_v4_frontend_plan(1120000u, false, 0x03);
    const auto vhf = measured_v4_frontend_plan(100000000u, false, 0x03);
    const auto uhf = measured_v4_frontend_plan(1090000000u, false, 0x03);
    EXPECT_EQ_U(hf.reg06, 0x38);
    EXPECT_EQ_U(hf.reg05, 0xa3);
    EXPECT_EQ_U(hf.gpo, 0x18);
    EXPECT_EQ_U(vhf.reg06, 0x30);
    EXPECT_EQ_U(vhf.reg05, 0xe3);
    EXPECT_EQ_U(vhf.gpo, 0x38);
    EXPECT_EQ_U(uhf.reg06, 0x30);
    EXPECT_EQ_U(uhf.reg05, 0x83);
    EXPECT_EQ_U(uhf.gpo, 0x38);
    EXPECT_EQ_U(measured_v4_frontend_plan(28800000u, false, 0x03).reg06, 0x38);
    EXPECT_TRUE(!esp_rtl_sdr_frequency_uses_hf_upconverter(28800000u));
}

/*
 * V4L identity and routing.
 *
 * The V4L carries an R828S, which answers over I2C where an R820T/R860
 * would. Detection therefore has to come from the EEPROM strings; relying on
 * the I2C probe brought the stick up as BlogV3, complete with a
 * direct-sampling HF path the board does not have.
 */
static void test_blog_v4l_identity(void)
{
    /* Exact-match on "Blog V4" used to reject "Blog V4L" and fall through to
     * the probe. It must resolve from descriptors alone. */
    EXPECT_TRUE(rtl_profile_from_descriptors(kRtlSharedVid, kRtlSharedPid,
                                             "RTLSDRBlog", "Blog V4L") ==
                RtlProfileId::BlogV4L);
    /* ...without stealing the plain V4. */
    EXPECT_TRUE(rtl_profile_from_descriptors(kRtlSharedVid, kRtlSharedPid,
                                             "RTLSDRBlog", "Blog V4") ==
                RtlProfileId::BlogV4);

    /* Even when the I2C probe says "R820T-like", descriptors win. */
    RtlProfileProbeResult v3_probe{};
    v3_probe.completed = true;
    v3_probe.chip_id = 0x69;
    EXPECT_TRUE(rtl_profile_select(kRtlSharedVid, kRtlSharedPid, "RTLSDRBlog",
                                   "Blog V4L", v3_probe) ==
                RtlProfileId::BlogV4L);

    /* R828S shares the remapped tuner addressing... */
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV4L));
    EXPECT_TRUE(rtl_profile_tuner_i2c_value(RtlProfileId::BlogV4L) ==
                kR820T2TunerI2cValue);

    /* ...but NOT the V3 HF handling. The V4L upconverts; folding it through
     * the Q-branch path would be actively wrong. */
    EXPECT_TRUE(!rtl_profile_uses_v3_direct_sampling(RtlProfileId::BlogV4L, 10000000u));
    EXPECT_TRUE((rtl_profile_device_capabilities(RtlProfileId::BlogV4L) &
                 ESP_RTL_SDR_CAP_DIRECT_SAMPLING) == 0);

    /* Nor does it claim the V4 front-end routing, which is a different board. */
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV4L));
    EXPECT_TRUE((rtl_profile_device_capabilities(RtlProfileId::BlogV4L) &
                 ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);

    /* HF fails closed until that upconverter's control is captured. */
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::BlogV4L, 10000000u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV4L, 100100000u));

    /* VHF/UHF streams, and keeps the cold tuner reinit the V4L was observed
     * streaming under while it was still misdetected as BlogV3. */
    EXPECT_TRUE(rtl_profile_supports_stream(RtlProfileId::BlogV4L));
    EXPECT_TRUE(rtl_profile_needs_cold_tuner_reinit(RtlProfileId::BlogV4L, 100100000u));

    /* V4 vendor board controls must not run on it. */
    RtlControlRecord vendor{};
    vendor.value = 0x3001;
    EXPECT_TRUE(!rtl_profile_allows_init_record(RtlProfileId::BlogV4L, vendor));
}

int main(void)
{
    test_detection_matrix();
    test_unknown_reject_and_v3_probe();
    test_tuner_isolation();
    test_frequency_policy();
    test_matched_if_policy();
    test_v3_direct_transition_records();
    test_capability_matrix();
    test_profile_transition_matrix();
    test_blog_v4l_identity();
    std::printf("RESULT profiles passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
