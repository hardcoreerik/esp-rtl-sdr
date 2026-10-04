/*
 * Host tests for the function-test firmware's pure parts: the JSON report
 * format and the per-dongle expectation table (examples/function_test/main).
 * The table is cross-checked against the driver's own profile code so it
 * cannot drift from what the library reports.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "esp_rtl_sdr.h"
#include "ft_core.hpp"
#include "ft_expect.hpp"
#include "gain_r820t2.hpp"
#include "measured_gain_bias_v4.hpp"
#include "rtl_profile.hpp"

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            g_failed++;                                                        \
        } else {                                                               \
            g_passed++;                                                        \
        }                                                                      \
    } while (0)

static void capture(const char *line, void *ctx)
{
    static_cast<std::vector<std::string> *>(ctx)->push_back(line);
}

static uint32_t g_now = 0;
static uint32_t fake_now() { return g_now; }

static void test_escape()
{
    char out[16];
    ft::json_escape(out, sizeof(out), "a\"b\\c\n");
    CHECK(std::strcmp(out, "a\\\"b\\\\c\\n") == 0);
    ft::json_escape(out, sizeof(out), "\x01\xff");
    CHECK(std::strcmp(out, "??") == 0);
    /* truncation keeps the string terminated and never splits an escape */
    ft::json_escape(out, 4, "ab\"c");
    CHECK(std::strcmp(out, "ab") == 0);
    ft::json_escape(out, sizeof(out), nullptr);
    CHECK(out[0] == '\0');
}

static void test_within_pct()
{
    CHECK(ft::within_pct(2400000, 2400000, 5));
    CHECK(ft::within_pct(2280000, 2400000, 5));
    CHECK(!ft::within_pct(2279999, 2400000, 5));
    CHECK(ft::within_pct(2520000, 2400000, 5));
    CHECK(!ft::within_pct(2520001, 2400000, 5));
    CHECK(ft::within_pct(0, 0, 5));
    CHECK(!ft::within_pct(1, 0, 5));
}

static void test_reporter()
{
    std::vector<std::string> lines;
    ft::Reporter r(capture, &lines);
    r.begin("0.9.3", "abc123", "test board");
    r.result("a.test", 0, "blog_v4_r828d", ft::Status::Pass, 12, "freq=%u", 99100000u);
    r.result("b.test", -1, nullptr, ft::Status::Fail, 3, "bad \"quote\"");
    r.result("c.test", 1, "x", ft::Status::Skip, 0, "no stimulus");
    r.summary(2);
    CHECK(lines.size() == 5);
    CHECK(lines[0] == "{\"ft\":\"begin\",\"version\":\"0.9.3\",\"sha\":\"abc123\",\"board\":\"test board\"}");
    CHECK(lines[1] == "{\"ft\":\"result\",\"test\":\"a.test\",\"dev\":0,\"profile\":\"blog_v4_r828d\","
                      "\"status\":\"PASS\",\"ms\":12,\"detail\":\"freq=99100000\"}");
    CHECK(lines[2].find("\"dev\":-1") != std::string::npos);
    CHECK(lines[2].find("\"profile\":\"\"") != std::string::npos);
    CHECK(lines[2].find("bad \\\"quote\\\"") != std::string::npos);
    CHECK(lines[4] == "{\"ft\":\"summary\",\"pass\":1,\"fail\":1,\"skip\":1,\"devices\":2,\"ok\":false}");
    CHECK(r.pass() == 1 && r.fail() == 1 && r.skip() == 1 && !r.ok());
}

static void test_ok_requires_a_pass()
{
    std::vector<std::string> lines;
    ft::Reporter r(capture, &lines);
    r.summary(0);
    CHECK(!r.ok()); /* an empty or all-skip run is not a pass */
    r.result("x", -1, "", ft::Status::Skip, 0, "s");
    CHECK(!r.ok());
    r.result("y", -1, "", ft::Status::Pass, 0, "p");
    CHECK(r.ok());
}

static void test_case()
{
    std::vector<std::string> lines;
    ft::Reporter r(capture, &lines);
    g_now = 100;
    {
        ft::Case c(r, "t.pass", 0, "p", fake_now);
        c.check(true, "unused");
        c.note("n=%d", 7);
        g_now = 150;
        c.finish();
    }
    CHECK(lines.back().find("\"status\":\"PASS\"") != std::string::npos);
    CHECK(lines.back().find("\"ms\":50") != std::string::npos);
    CHECK(lines.back().find("\"detail\":\"n=7\"") != std::string::npos);
    {
        ft::Case c(r, "t.fail", 0, "p", fake_now);
        c.check(false, "first %d", 1);
        c.check(false, "second");
        c.check(false, "third");
        c.finish();
    }
    CHECK(lines.back().find("\"status\":\"FAIL\"") != std::string::npos);
    CHECK(lines.back().find("first 1 (+2 more)") != std::string::npos);
    {
        ft::Case c(r, "t.skip", 0, "p", fake_now);
        c.skip("because %s", "reason");
        c.finish(); /* no second result */
    }
    CHECK(lines.back().find("\"status\":\"SKIP\"") != std::string::npos);
    const size_t before = lines.size();
    {
        ft::Case c(r, "t.forgot", 0, "p", fake_now);
        (void)c; /* destroyed without finish() */
    }
    CHECK(lines.size() == before + 1);
    CHECK(lines.back().find("\"status\":\"FAIL\"") != std::string::npos);
    CHECK(lines.back().find("without a verdict") != std::string::npos);
}

static RtlProfileId to_internal(esp_rtl_sdr_profile_t p) { return static_cast<RtlProfileId>(p); }

static void test_expect_matches_driver()
{
    CHECK(ft::find_expect(ESP_RTL_SDR_PROFILE_UNKNOWN) == nullptr);
    for (const ft::Expect &e : ft::kExpect) {
        const RtlProfileId id = to_internal(e.profile);
        const uint32_t caps = rtl_profile_device_capabilities(id);
        if (e.caps_required != caps) {
            std::printf("  caps mismatch %s: table=%08x driver=%08x\n",
                        rtl_profile_name(id), static_cast<unsigned>(e.caps_required),
                        static_cast<unsigned>(caps));
        }
        CHECK(e.caps_required == caps);
        CHECK((e.caps_forbidden & caps) == 0);
        CHECK((e.caps_required & e.caps_forbidden) == 0);
        CHECK(rtl_profile_supports_rf_hz(id, e.rf_min_hz));
        CHECK(rtl_profile_supports_rf_hz(id, e.rf_max_hz));
        CHECK(!rtl_profile_supports_rf_hz(id, e.rf_max_hz + 1u));
        if (e.rf_min_hz > ESP_RTL_SDR_FREQ_MIN_HZ) {
            CHECK(!rtl_profile_supports_rf_hz(id, e.rf_min_hz - 1u));
        }
        const bool v4_family =
            id == RtlProfileId::BlogV4 || id == RtlProfileId::BlogV4L;
        CHECK(e.hf_direct_route == v4_family);
        const size_t steps =
            id == RtlProfileId::BlogV4 ? kMeasuredV4GainStepCount : kR820T2GainStepCount;
        if (e.gain_steps != steps) {
            std::printf("  gain steps mismatch %s: table=%u driver=%u\n", rtl_profile_name(id),
                        static_cast<unsigned>(e.gain_steps), static_cast<unsigned>(steps));
        }
        CHECK(e.gain_steps == steps);
    }
    /* every accepted public profile has a row */
    CHECK(ft::find_expect(ESP_RTL_SDR_PROFILE_BLOG_V4) != nullptr);
    CHECK(ft::find_expect(ESP_RTL_SDR_PROFILE_BLOG_V4L) != nullptr);
    CHECK(ft::find_expect(ESP_RTL_SDR_PROFILE_BLOG_V3) != nullptr);
    CHECK(ft::find_expect(ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5) != nullptr);
}

int main()
{
    test_escape();
    test_within_pct();
    test_reporter();
    test_ok_requires_a_pass();
    test_case();
    test_expect_matches_driver();
    std::printf("function_test host: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
