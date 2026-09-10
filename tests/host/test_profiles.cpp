/*
 * Behavioral host tests for multi-dongle identity / caps / frequency policy.
 * No USB / FreeRTOS. Pure profile + V4 frontend composition + tuner isolation.
 */
#include "esp_rtl_sdr.h"
#include "measured_gain_bias_v4.hpp"
#include "rtl_profile.hpp"
#include "transfers_blog_v3.hpp"
#include "transfers_blog_v4.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

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
    /* V4: HF upconverter + full span. */
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV4, 1120000u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV4, 100000000u));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV4, 10000000u),
                10000000u + 28800000u);
    EXPECT_TRUE(rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV4));

    /* Nooelec: reject < 24 MHz; no V4 HF LO offset / routing. */
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 10000000u));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 23999999u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 24000000u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::NooelecSmartV5, 100000000u));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::NooelecSmartV5, 100000000u),
                100000000u);
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::NooelecSmartV5));

    /* V3 provisional: same R820T2 floor as Nooelec; no V4 HF LO offset. */
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, 10000000u));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, 23999999u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, 24000000u));
    EXPECT_TRUE(rtl_profile_supports_rf_hz(RtlProfileId::BlogV3, 100000000u));
    EXPECT_TRUE(rtl_profile_supports_stream(RtlProfileId::BlogV3));
    EXPECT_EQ_U(rtl_profile_tuner_frequency_hz(RtlProfileId::BlogV3, 100000000u),
                100000000u);
    EXPECT_TRUE(!rtl_profile_uses_v4_hf_routing(RtlProfileId::BlogV3));
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV3));
    EXPECT_TRUE(rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::NooelecSmartV5));
    EXPECT_TRUE(!rtl_profile_uses_r820t2_i2c_remap(RtlProfileId::BlogV4));
    EXPECT_TRUE(!rtl_profile_supports_rf_hz(RtlProfileId::Unknown, 100000000u));
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

    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_RETUNE) != 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_GAIN) == 0);
    EXPECT_TRUE((v3 & ESP_RTL_SDR_CAP_BIAS_TEE) == 0);

    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_STREAM) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_RETUNE) != 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_HF_UPCONVERTER) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_GAIN) == 0);
    EXPECT_TRUE((noe & ESP_RTL_SDR_CAP_BIAS_TEE) == 0);

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

int main(void)
{
    test_detection_matrix();
    test_unknown_reject_and_v3_probe();
    test_tuner_isolation();
    test_frequency_policy();
    test_capability_matrix();
    test_profile_transition_matrix();
    std::printf("RESULT profiles passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
