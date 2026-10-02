/*
 * Host tests for private/r820t2_band.hpp: the RF mux / tracking-filter band table, replayed against the registers
 * our own PC captures of the vendor DLL (2026-10-01) show it leaving behind on a Nooelec SMArt v5 and a Blog V3c.
 * No USB stack, FreeRTOS or ESP-IDF.
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

/* 47 tunes each, ascending, from the 2026-10-01 band sweeps (both sides of every boundary). */
static const Captured kNooelec[] = {
    {26430000u, 0x28, 0x2a, 0xdf},
    {40000000u, 0x28, 0x2a, 0xdf},
    {46130000u, 0x28, 0x2a, 0xdf},
    {46730000u, 0x28, 0x2a, 0xbe},
    {51130000u, 0x28, 0x2a, 0xbe},
    {51730000u, 0x28, 0x2a, 0x8b},
    {56130000u, 0x28, 0x2a, 0x8b},
    {56730000u, 0x28, 0x2a, 0x7b},
    {61130000u, 0x28, 0x2a, 0x7b},
    {61730000u, 0x28, 0x2a, 0x69},
    {66130000u, 0x28, 0x2a, 0x69},
    {66730000u, 0x28, 0x2a, 0x58},
    {71130000u, 0x28, 0x2a, 0x58},
    {71730000u, 0x20, 0x2a, 0x44},
    {76130000u, 0x20, 0x2a, 0x44},
    {76730000u, 0x20, 0x2a, 0x44},
    {86130000u, 0x20, 0x2a, 0x44},
    {86730000u, 0x20, 0x2a, 0x34},
    {96130000u, 0x20, 0x2a, 0x34},
    {96730000u, 0x20, 0x2a, 0x34},
    {106130000u, 0x20, 0x2a, 0x34},
    {106730000u, 0x20, 0x2a, 0x24},
    {116130000u, 0x20, 0x2a, 0x24},
    {116730000u, 0x20, 0x2a, 0x24},
    {136130000u, 0x20, 0x2a, 0x24},
    {136730000u, 0x20, 0x2a, 0x14},
    {176130000u, 0x20, 0x2a, 0x14},
    {176730000u, 0x20, 0x2a, 0x13},
    {216130000u, 0x20, 0x2a, 0x13},
    {216730000u, 0x20, 0x2a, 0x13},
    {246130000u, 0x20, 0x2a, 0x13},
    {246730000u, 0x20, 0x2a, 0x11},
    {276130000u, 0x20, 0x2a, 0x11},
    {276730000u, 0x20, 0x2a, 0x00},
    {306130000u, 0x20, 0x2a, 0x00},
    {306730000u, 0x20, 0x69, 0x00},
    {433920000u, 0x20, 0x69, 0x00},
    {446130000u, 0x20, 0x69, 0x00},
    {446730000u, 0x20, 0x69, 0x00},
    {584130000u, 0x20, 0x69, 0x00},
    {584730000u, 0x20, 0x68, 0x00},
    {646130000u, 0x20, 0x68, 0x00},
    {646730000u, 0x20, 0x68, 0x00},
    {915000000u, 0x20, 0x68, 0x00},
    {1000000000u, 0x20, 0x68, 0x00},
    {1500000000u, 0x20, 0x68, 0x00},
    {1700000000u, 0x20, 0x68, 0x00},
};
static const Captured kV3c[] = {
    {26430000u, 0x28, 0x2a, 0xdf},
    {40000000u, 0x28, 0x2a, 0xdf},
    {46130000u, 0x28, 0x2a, 0xdf},
    {46730000u, 0x28, 0x2a, 0xbe},
    {51130000u, 0x28, 0x2a, 0xbe},
    {51730000u, 0x28, 0x2a, 0x8b},
    {56130000u, 0x28, 0x2a, 0x8b},
    {56730000u, 0x28, 0x2a, 0x7b},
    {61130000u, 0x28, 0x2a, 0x7b},
    {61730000u, 0x28, 0x2a, 0x69},
    {66130000u, 0x28, 0x2a, 0x69},
    {66730000u, 0x28, 0x2a, 0x58},
    {71130000u, 0x28, 0x2a, 0x58},
    {71730000u, 0x20, 0x2a, 0x44},
    {76130000u, 0x20, 0x2a, 0x44},
    {76730000u, 0x20, 0x2a, 0x44},
    {86130000u, 0x20, 0x2a, 0x44},
    {86730000u, 0x20, 0x2a, 0x34},
    {96130000u, 0x20, 0x2a, 0x34},
    {96730000u, 0x20, 0x2a, 0x34},
    {106130000u, 0x20, 0x2a, 0x34},
    {106730000u, 0x20, 0x2a, 0x24},
    {116130000u, 0x20, 0x2a, 0x24},
    {116730000u, 0x20, 0x2a, 0x24},
    {136130000u, 0x20, 0x2a, 0x24},
    {136730000u, 0x20, 0x2a, 0x14},
    {176130000u, 0x20, 0x2a, 0x14},
    {176730000u, 0x20, 0x2a, 0x13},
    {216130000u, 0x20, 0x2a, 0x13},
    {216730000u, 0x20, 0x2a, 0x13},
    {246130000u, 0x20, 0x2a, 0x13},
    {246730000u, 0x20, 0x2a, 0x11},
    {276130000u, 0x20, 0x2a, 0x11},
    {276730000u, 0x20, 0x2a, 0x00},
    {306130000u, 0x20, 0x2a, 0x00},
    {306730000u, 0x20, 0x69, 0x00},
    {433920000u, 0x20, 0x69, 0x00},
    {446130000u, 0x20, 0x69, 0x00},
    {446730000u, 0x20, 0x69, 0x00},
    {584130000u, 0x20, 0x69, 0x00},
    {584730000u, 0x20, 0x68, 0x00},
    {646130000u, 0x20, 0x68, 0x00},
    {646730000u, 0x20, 0x68, 0x00},
    {915000000u, 0x20, 0x68, 0x00},
    {1000000000u, 0x20, 0x68, 0x00},
    {1500000000u, 0x20, 0x68, 0x00},
    {1700000000u, 0x20, 0x68, 0x00},
};

/* A tuner register write as the FM-captured tune template issues it. */
static RtlControlRecord write_reg(uint8_t reg, uint8_t value)
{
    RtlControlRecord rec{};
    rec.value = 0x0034;
    rec.index = 0x0610;
    rec.request_type = 0x40;
    rec.length = 2;
    rec.data[0] = reg;
    rec.data[1] = value;
    return rec;
}

static void final_registers(uint32_t rf_hz, uint8_t out[3])
{
    /* The Nooelec and V3c PLL runs at RF + 3.57 MHz. */
    const r820t2::Band &band = r820t2::band_for_lo_hz(r820t2::lo_hz(rf_hz, 3570000.0));
    RtlControlRecord r17 = write_reg(0x17, 0x20), r1a = write_reg(0x1a, 0x2a), r1b = write_reg(0x1b, 0x34);
    r820t2::patch_record(band, r17);
    r820t2::patch_record(band, r1a);
    r820t2::patch_record(band, r1b);
    out[0] = r17.data[1];
    out[1] = r1a.data[1];
    out[2] = r1b.data[1];
}

static void replay(const Captured *points, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        uint8_t got[3];
        final_registers(points[i].rf_hz, got);
        CHECK(got[0] == points[i].r17);
        CHECK(got[1] == points[i].r1a);
        CHECK(got[2] == points[i].r1b);
    }
}

static void test_table_shape()
{
    CHECK(r820t2::kBands[0].from_mhz == 0);
    for (size_t i = 1; i < r820t2::kBandCount; ++i) {
        CHECK(r820t2::kBands[i].from_mhz > r820t2::kBands[i - 1].from_mhz);
        /* a row exists only where something changes */
        const auto &a = r820t2::kBands[i - 1];
        const auto &b = r820t2::kBands[i];
        CHECK(a.r17_open_drain != b.r17_open_drain || a.r1a_mux != b.r1a_mux || a.r1b_filter != b.r1b_filter);
    }
    /* only the bits the masks cover are ever set */
    for (const auto &row : r820t2::kBands) {
        CHECK((row.r17_open_drain & ~r820t2::kR17Mask) == 0);
        CHECK((row.r1a_mux & ~r820t2::kR1aMask) == 0);
    }
}

static void test_lookup_edges()
{
    /* exactly on a row start selects that row; one hertz below selects the previous one */
    for (size_t i = 1; i < r820t2::kBandCount; ++i) {
        const uint32_t edge = static_cast<uint32_t>(r820t2::kBands[i].from_mhz) * 1000000u;
        CHECK(&r820t2::band_for_lo_hz(edge) == &r820t2::kBands[i]);
        CHECK(&r820t2::band_for_lo_hz(edge - 1) == &r820t2::kBands[i - 1]);
    }
    CHECK(&r820t2::band_for_lo_hz(0) == &r820t2::kBands[0]);
    CHECK(&r820t2::band_for_lo_hz(2000000000u) == &r820t2::kBands[r820t2::kBandCount - 1]);
    /* keyed on the LO, not the RF: 307 MHz is below the 310 MHz edge with a 1.8 MHz IF and above it with 3.57 MHz */
    CHECK(r820t2::band_for_lo_hz(r820t2::lo_hz(307000000u, 1814972.0)).from_mhz == 280);
    CHECK(r820t2::band_for_lo_hz(r820t2::lo_hz(307000000u, 3570000.0)).from_mhz == 310);
}

static void test_patch_touches_only_band_registers()
{
    const r820t2::Band &band = r820t2::band_for_lo_hz(437000000u);
    RtlControlRecord other = write_reg(0x0c, 0x68);
    r820t2::patch_record(band, other);
    CHECK(other.data[0] == 0x0c && other.data[1] == 0x68);
    RtlControlRecord wrong_length = write_reg(0x1b, 0x34);
    wrong_length.length = 1;
    r820t2::patch_record(band, wrong_length);
    CHECK(wrong_length.data[1] == 0x34);
    RtlControlRecord wrong_request = write_reg(0x1b, 0x34);
    wrong_request.request_type = 0xc0;
    r820t2::patch_record(band, wrong_request);
    CHECK(wrong_request.data[1] == 0x34);
    RtlControlRecord wrong_index = write_reg(0x1b, 0x34);
    wrong_index.index = 0x0600;
    r820t2::patch_record(band, wrong_index);
    CHECK(wrong_index.data[1] == 0x34);
    /* bits outside the masks survive: register 1a keeps its PLL autotune bits */
    RtlControlRecord r1a = write_reg(0x1a, 0x22);
    r820t2::patch_record(r820t2::band_for_lo_hz(437000000u), r1a);
    CHECK(r1a.data[1] == 0x61);
}

static void test_profiles()
{
    CHECK(r820t2::profile_uses_band_select(RtlProfileId::BlogV3));
    CHECK(r820t2::profile_uses_band_select(RtlProfileId::NooelecSmartV5));
    CHECK(!r820t2::profile_uses_band_select(RtlProfileId::BlogV4));
    CHECK(!r820t2::profile_uses_band_select(RtlProfileId::BlogV4L));
    CHECK(!r820t2::profile_uses_band_select(RtlProfileId::Unknown));
}

int main()
{
    test_table_shape();
    test_lookup_edges();
    test_patch_touches_only_band_registers();
    test_profiles();
    replay(kNooelec, sizeof(kNooelec) / sizeof(kNooelec[0]));
    replay(kV3c, sizeof(kV3c) / sizeof(kV3c[0]));
    std::printf("RESULT r820t2_band passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
