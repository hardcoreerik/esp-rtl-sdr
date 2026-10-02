/*
 * Host tests for the Blog V4 / V4L band registers in private/r820t2_band.hpp, replayed against our own 2026-10-01 PC captures
 * of the vendor DLL (docs/captures/band_sweep_2026-10-01/blog_v4.csv and blog_v4l.csv). The DLL ran at 2.048 MS/s, where
 * its IF is 1.625 MHz, so LO = RF + 1.625 MHz here; the driver keys on the IF it actually programs.
 */

#include <cstdint>
#include <cstdio>

#include "r820t2_band.hpp"

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

struct Captured {
    uint32_t rf_hz;
    uint8_t r17, r1a, r1b;
};

static const Captured kV4[] = {
    {40000000u, 0x28, 0x2a, 0xdf},
    {46130000u, 0x28, 0x2a, 0xdf},
    {46730000u, 0x28, 0x2a, 0xdf},
    {51130000u, 0x28, 0x2a, 0xbe},
    {51730000u, 0x28, 0x2a, 0xbe},
    {56130000u, 0x28, 0x2a, 0x8b},
    {56730000u, 0x28, 0x2a, 0x8b},
    {61130000u, 0x28, 0x2a, 0x7b},
    {61730000u, 0x28, 0x2a, 0x7b},
    {66130000u, 0x28, 0x2a, 0x69},
    {66730000u, 0x28, 0x2a, 0x69},
    {71130000u, 0x28, 0x2a, 0x58},
    {71730000u, 0x28, 0x2a, 0x58},
    {76130000u, 0x28, 0x2a, 0x44},
    {76730000u, 0x28, 0x2a, 0x44},
    {86130000u, 0x20, 0x2a, 0x44},
    {86730000u, 0x20, 0x2a, 0x44},
    {96130000u, 0x20, 0x2a, 0x34},
    {96730000u, 0x20, 0x2a, 0x34},
    {106130000u, 0x20, 0x2a, 0x34},
    {106730000u, 0x20, 0x2a, 0x34},
    {116130000u, 0x28, 0x2a, 0x24},
    {116730000u, 0x28, 0x2a, 0x24},
    {136130000u, 0x28, 0x2a, 0x24},
    {136730000u, 0x28, 0x2a, 0x24},
    {176130000u, 0x20, 0x2a, 0x14},
    {176730000u, 0x20, 0x2a, 0x14},
    {216130000u, 0x20, 0x2a, 0x13},
    {216730000u, 0x20, 0x2a, 0x13},
    {246130000u, 0x28, 0x2a, 0x13},
    {246730000u, 0x28, 0x2a, 0x13},
    {276130000u, 0x28, 0x2a, 0x11},
    {276730000u, 0x28, 0x2a, 0x11},
    {306130000u, 0x28, 0x2a, 0x00},
    {306730000u, 0x28, 0x2a, 0x00},
    {433920000u, 0x28, 0x69, 0x00},
    {446130000u, 0x28, 0x69, 0x00},
    {446730000u, 0x28, 0x69, 0x00},
    {584130000u, 0x28, 0x69, 0x00},
    {584730000u, 0x28, 0x69, 0x00},
    {646130000u, 0x28, 0x68, 0x00},
    {646730000u, 0x28, 0x68, 0x00},
    {915000000u, 0x28, 0x68, 0x00},
    {1000000000u, 0x28, 0x68, 0x00},
    {1500000000u, 0x28, 0x68, 0x00},
    {1700000000u, 0x28, 0x68, 0x00},
};
static const Captured kV4L[] = {
    {40000000u, 0x28, 0x2a, 0xdf},
    {46130000u, 0x28, 0x2a, 0xdf},
    {46730000u, 0x28, 0x2a, 0xdf},
    {51130000u, 0x28, 0x2a, 0xbe},
    {51730000u, 0x28, 0x2a, 0xbe},
    {56130000u, 0x28, 0x2a, 0x8b},
    {56730000u, 0x28, 0x2a, 0x8b},
    {61130000u, 0x28, 0x2a, 0x7b},
    {61730000u, 0x28, 0x2a, 0x7b},
    {66130000u, 0x28, 0x2a, 0x69},
    {66730000u, 0x28, 0x2a, 0x69},
    {71130000u, 0x28, 0x2a, 0x58},
    {71730000u, 0x28, 0x2a, 0x58},
    {76130000u, 0x20, 0x2a, 0x44},
    {76730000u, 0x20, 0x2a, 0x44},
    {86130000u, 0x20, 0x2a, 0x44},
    {86730000u, 0x20, 0x2a, 0x44},
    {96130000u, 0x20, 0x2a, 0x34},
    {96730000u, 0x20, 0x2a, 0x34},
    {106130000u, 0x20, 0x2a, 0x34},
    {106730000u, 0x20, 0x2a, 0x34},
    {116130000u, 0x20, 0x2a, 0x24},
    {116730000u, 0x20, 0x2a, 0x24},
    {136130000u, 0x20, 0x2a, 0x24},
    {136730000u, 0x20, 0x2a, 0x24},
    {176130000u, 0x20, 0x2a, 0x14},
    {176730000u, 0x20, 0x2a, 0x14},
    {216130000u, 0x20, 0x2a, 0x13},
    {216730000u, 0x20, 0x2a, 0x13},
    {246130000u, 0x20, 0x2a, 0x13},
    {246730000u, 0x20, 0x2a, 0x13},
    {276130000u, 0x20, 0x2a, 0x11},
    {276730000u, 0x20, 0x2a, 0x11},
    {306130000u, 0x20, 0x2a, 0x00},
    {306730000u, 0x20, 0x2a, 0x00},
    {433920000u, 0x20, 0x69, 0x00},
    {446130000u, 0x20, 0x69, 0x00},
    {446730000u, 0x20, 0x69, 0x00},
    {584130000u, 0x20, 0x69, 0x00},
    {584730000u, 0x20, 0x69, 0x00},
    {646130000u, 0x20, 0x68, 0x00},
    {646730000u, 0x20, 0x68, 0x00},
    {915000000u, 0x20, 0x68, 0x00},
    {1000000000u, 0x20, 0x68, 0x00},
    {1500000000u, 0x20, 0x68, 0x00},
    {1700000000u, 0x20, 0x68, 0x00},
};

constexpr uint32_t kCaptureIfHz = 1625000u;

static void test_v4_replay()
{
    for (const Captured &p : kV4) {
        r820t2::V4Regs regs{};
        CHECK(r820t2::v4_regs(p.rf_hz, p.rf_hz + kCaptureIfHz, &regs));
        CHECK(regs.r17 == p.r17);
        CHECK(regs.r1a == p.r1a);
        CHECK(regs.r1b == p.r1b);
    }
}

/* The V4L has no register-17 pattern of its own: it takes the R820T2 row for the LO. */
static void test_v4l_replay()
{
    for (const Captured &p : kV4L) {
        CHECK(r820t2::v4l_uses_band_select(RtlProfileId::BlogV4L, p.rf_hz));
        const r820t2::Band &band = r820t2::band_for_lo_hz(p.rf_hz + kCaptureIfHz);
        const uint8_t r17 = static_cast<uint8_t>((0x20 & ~r820t2::kR17Mask) | band.r17_open_drain);
        const uint8_t r1a = static_cast<uint8_t>((0x2a & ~r820t2::kR1aMask) | band.r1a_mux);
        CHECK(r17 == p.r17);
        CHECK(r1a == p.r1a);
        CHECK(band.r1b_filter == p.r1b);
    }
}

static void test_v4_open_drain_edges()
{
    r820t2::V4Regs regs{};
    const auto r17_at = [&](uint32_t rf) {
        CHECK(r820t2::v4_regs(rf, rf + 1814972u, &regs));
        return regs.r17;
    };
    CHECK(r17_at(84999999u) == 0x28);
    CHECK(r17_at(85000000u) == 0x20);   /* inclusive */
    CHECK(r17_at(112000000u) == 0x20);  /* exclusive: still the earlier value */
    CHECK(r17_at(112000001u) == 0x28);
    CHECK(r17_at(171999999u) == 0x28);
    CHECK(r17_at(172000000u) == 0x20);  /* inclusive */
    CHECK(r17_at(242000000u) == 0x20);  /* exclusive */
    CHECK(r17_at(242000001u) == 0x28);
    CHECK(r17_at(1090000000u) == 0x28); /* the ADS-B band keeps what it always had */
    CHECK(r17_at(96100000u) == 0x20);   /* and so does FM */
}

/* The row follows the LO the PLL is programmed to, which includes the ppm correction (CodeRabbit, PR #54). At 307.9 MHz
 * with +1000 ppm the corrected tuner frequency is 308.2079 MHz and the LO 310.02 MHz, past the 310 MHz row boundary,
 * while the uncorrected LO (309.71 MHz) is still in the 280 MHz row. */
static void test_row_follows_corrected_lo()
{
    const uint32_t rf = 307900000u, if_hz = 1814972u;
    const uint32_t corrected_tuner = rf + rf / 1000u;
    r820t2::V4Regs plain{}, corrected{};
    CHECK(r820t2::v4_regs(rf, rf + if_hz, &plain));
    CHECK(r820t2::v4_regs(rf, corrected_tuner + if_hz, &corrected));
    CHECK(plain.r1a == 0x2a && plain.r1b == 0x00);      /* 280 MHz row: 1a = 0x28 | mux 0x02 */
    CHECK(corrected.r1a == 0x69 && corrected.r1b == 0x00); /* 310 MHz row: 1a = 0x28 | mux 0x41 */
}

static void test_scope()
{
    r820t2::V4Regs regs{};
    /* the HF upconverter route and the 28.8 MHz boundary are not touched */
    CHECK(!r820t2::v4_regs(28800000u, 30600000u, &regs));
    CHECK(!r820t2::v4_regs(10000000u, 11800000u, &regs));
    CHECK(!r820t2::v4_regs(96100000u, 97900000u, nullptr));
    CHECK(!r820t2::v4l_uses_band_select(RtlProfileId::BlogV4L, 28800000u));
    CHECK(!r820t2::v4l_uses_band_select(RtlProfileId::BlogV4, 433920000u));
    CHECK(!r820t2::v4l_uses_band_select(RtlProfileId::BlogV3, 433920000u));
    /* FM and ADS-B on the V4 get exactly the registers they got before: 96.1 MHz is 20/2a/34, 1090 MHz is 28/68/00 */
    CHECK(r820t2::v4_regs(96100000u, 96100000u + 1814972u, &regs) && regs.r17 == 0x20 && regs.r1a == 0x2a && regs.r1b == 0x34);
    CHECK(r820t2::v4_regs(1090000000u, 1090000000u + 1814972u, &regs) && regs.r17 == 0x28 && regs.r1a == 0x68 && regs.r1b == 0x00);
}

int main()
{
    test_v4_replay();
    test_v4l_replay();
    test_v4_open_drain_edges();
    test_row_follows_corrected_lo();
    test_scope();
    std::printf("RESULT v4_band passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
