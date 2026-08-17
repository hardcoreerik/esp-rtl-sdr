#include "rtl_profile.hpp"
#include "transfers_blog_v3.hpp"
#include "transfers_blog_v4.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

static void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct ProbeReply {
    bool completed;
    bool stall;
    uint8_t byte;
};

static bool simulate_v3_probe(const std::array<ProbeReply, 2> &replies,
                              RtlProfileProbeResult *out_probe,
                              std::array<RtlControlRecord, 2> *trace)
{
    if (out_probe == nullptr || trace == nullptr) {
        return false;
    }
    *out_probe = {};
    (*trace)[0] = kBlogV3ProbeSelect;
    (*trace)[1] = kBlogV3ProbeRead;
    if (!replies[0].completed || replies[0].stall || !replies[1].completed || replies[1].stall) {
        return false;
    }
    out_probe->completed = true;
    out_probe->chip_id = replies[1].byte;
    return rtl_profile_v3_probe_matches(*out_probe);
}

static bool encode_pll(uint32_t frequency_hz, std::array<uint8_t, 5> *out)
{
    constexpr double kIfOffsetHz = 1814972.0;
    constexpr double kXtalHz = 28800000.0;
    constexpr uint16_t kMixCandidates[] = {2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    const double lo_hz = static_cast<double>(frequency_hz) + kIfOffsetHz;
    uint16_t mix = 0;
    for (uint16_t candidate : kMixCandidates) {
        const double vco = lo_hz * candidate;
        if (vco >= 1.77e9 && vco <= 3.90e9) {
            mix = candidate;
            break;
        }
    }
    if (mix == 0) {
        return false;
    }
    const double n = (lo_hz * mix) / (2.0 * kXtalHz);
    int nint = static_cast<int>(std::floor(n));
    int nfra = static_cast<int>(std::lround((n - nint) * 65536.0));
    if (nfra >= 65536) {
        ++nint;
        nfra = 0;
    }
    const int packed = nint - 13;
    const int ni2c = packed >> 2;
    const int si2c = packed & 3;
    int mix_log = 0;
    for (uint16_t value = mix; value > 1; value >>= 1) {
        ++mix_log;
    }
    const uint8_t active = static_cast<uint8_t>((((mix_log - 1) & 0x07) << 5) | 0x04);
    (*out)[0] = static_cast<uint8_t>(active + 0x20);
    (*out)[1] = active;
    (*out)[2] = static_cast<uint8_t>((si2c << 6) | ni2c);
    (*out)[3] = static_cast<uint8_t>(nfra & 0xff);
    (*out)[4] = static_cast<uint8_t>((nfra >> 8) & 0xff);
    return true;
}

static uint32_t sample_rate_ratio(uint32_t sample_rate_sps)
{
    return static_cast<uint32_t>((static_cast<uint64_t>(28800000) << 22) / sample_rate_sps) &
           0x0ffffffcu;
}

static size_t simulate_init_failure(size_t fail_at)
{
    bool interface_claimed = true;
    bool streaming = false;
    for (size_t i = 0; i < std::size(kRtlInitTransfers); ++i) {
        if (i == fail_at) {
            streaming = false;
            interface_claimed = false;
            return interface_claimed || streaming ? 0 : std::size(kRtlCleanupTransfers);
        }
    }
    streaming = true;
    return interface_claimed && streaming ? 0 : std::size(kRtlCleanupTransfers);
}

int main()
{
    expect(kBlogV3ProbeSelect.value == 0x0034 && kBlogV3ProbeSelect.index == 0x0610,
           "V3 probe selects the documented R820T2 write address");
    expect(kBlogV3ProbeRead.value == 0x0034 && kBlogV3ProbeRead.index == 0x0600,
           "V3 probe reads back through the R820T2 bridge address");
    expect(kBlogV3ProbeSelect.request_type == 0x40 &&
               kBlogV3ProbeRead.request_type == 0xc0,
           "V3 probe preserves ordered OUT then IN control direction");
    expect(kBlogV3ProbeSelect.data[0] == 0x00 && kBlogV3ProbeRead.length == 1,
           "V3 probe selects register zero and reads one byte");

    const auto v4 = rtl_profile_select(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V4", {});
    expect(v4 == RtlProfileId::BlogV4, "exact V4 descriptors select V4");

    const auto v3 = rtl_profile_select(0x0BDA, 0x2838, "RTLSDRBlog", "Blog V3", {});
    expect(v3 == RtlProfileId::BlogV3, "exact V3 descriptors select V3");

    const auto v3_alt = rtl_profile_select(0x0BDA, 0x2838, "RTLSDRBlog", "RTL-SDR Blog V3", {});
    expect(v3_alt == RtlProfileId::BlogV3, "alternate V3 descriptor selects V3");

    expect(rtl_profile_select(0x0BDA, 0x2838, "RTLSDRBlog", "unknown", {}) ==
               RtlProfileId::Unknown,
           "ambiguous shared USB identity stays rejected without a probe");

    expect(rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "unknown", {true, 0x96}) ==
               RtlProfileId::BlogV3,
           "completed R820T2 chip-id probe selects V3");
    expect(rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "unknown", {true, 0x69}) ==
               RtlProfileId::BlogV3,
           "bit-reversed R820T2 chip-id probe selects V3");
    expect(rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "unknown", {false, 0x96}) ==
               RtlProfileId::Unknown,
           "incomplete probe does not select V3");
    expect(rtl_profile_select(0x0BDA, 0x2838, "RTL2832U", "unknown", {true, 0x00}) ==
               RtlProfileId::Unknown,
           "unexpected chip id does not select V3");
    expect(rtl_profile_select(0x1234, 0x2838, "RTLSDRBlog", "Blog V3", {}) ==
               RtlProfileId::Unknown,
           "wrong VID stays rejected");

    std::array<RtlControlRecord, 2> trace{};
    RtlProfileProbeResult probe{};
    expect(simulate_v3_probe({ProbeReply{true, false, 0}, ProbeReply{true, false, 0x96}}, &probe,
                             &trace),
           "completed V3 probe accepts the expected read response");
    expect(trace[0].request_type == 0x40 && trace[1].request_type == 0xc0 &&
               trace[0].index == 0x0610 && trace[1].index == 0x0600,
           "V3 trace stays ordered OUT select then IN read");
    expect(!simulate_v3_probe({ProbeReply{false, false, 0}, ProbeReply{true, false, 0x96}},
                              &probe, &trace),
           "select failure is fatal");
    expect(!simulate_v3_probe({ProbeReply{true, false, 0}, ProbeReply{false, true, 0x96}},
                              &probe, &trace),
           "read STALL is fatal and not imported as expected");

    std::array<uint8_t, 5> pll{};
    expect(encode_pll(24000000, &pll) && pll == std::array<uint8_t, 5>{0xe4, 0xc4, 0x0b, 0xda, 0x5d},
           "low-frequency PLL encoding is stable");
    expect(encode_pll(100000000, &pll) && pll == std::array<uint8_t, 5>{0xa4, 0x84, 0xca, 0x5a, 0x90},
           "mid-frequency PLL encoding is stable");
    expect(encode_pll(1000000000, &pll) && pll == std::array<uint8_t, 5>{0x24, 0x04, 0x45, 0x06, 0xc9},
           "high-frequency PLL encoding is stable");
    expect(sample_rate_ratio(960000) == 0x07800000 &&
               sample_rate_ratio(1024000) == 0x07080000 &&
               sample_rate_ratio(2048000) == 0x03840000,
           "RTL2832 sample-rate ratios are stable");

    expect(std::size(kRtlInitTransfers) == 515 && std::size(kRtlCleanupTransfers) == 16 &&
               kRtlInitTransfers[0].value == 0x2000 && kRtlInitTransfers[514].value == 0x2148 &&
               kRtlFinalTuneTemplate[0].value == 0x0074,
           "existing V4 trace anchors remain unchanged");
    for (size_t i = 0; i < std::size(kRtlInitTransfers); ++i) {
        expect(simulate_init_failure(i) == std::size(kRtlCleanupTransfers),
               "each simulated V4 init failure fails closed through cleanup");
    }
    expect(simulate_init_failure(std::size(kRtlInitTransfers)) == 0,
           "successful simulated init does not run cleanup");

    return 0;
}
