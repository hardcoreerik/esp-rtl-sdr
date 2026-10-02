#pragma once

/**
 * R820T2-family RF mux and tracking-filter ("band") registers.
 *
 * The tune sequence this driver replays was captured on FM, so every tune left registers 17, 1a and 1b on the
 * 90-110 MHz setting and the front end was effectively deaf away from FM (issue #25).
 *
 * Provenance: every row below is a transition we observed in our own black-box PC captures of the vendor DLL
 * (2026-10-01; docs/captures/nooelec_v5_band_sweep_2026-10-01.md and docs/captures/band_sweep_2026-10-01/): 47 tunes up
 * and down on a Nooelec SMArt v5 and a Blog V3c, both sides of every boundary, plus 10 kHz scans of each edge on the V4
 * and V4L. A row is listed only where the registers changed, so the table has 15 rows. The sweeps also tuned both sides of the
 * boundaries at 80, 100, 120, 220, 450 and 650 MHz LO and measured no change there, so those are not rows. The switch points are in the LO
 * (tuner frequency plus the IF the PLL is using), not the RF.
 *
 *   17 bit 3  : input open-drain, set below 75 MHz
 *   1a & 0xc3 : RF mux / polyphase bits
 *   1b        : tracking-filter code
 */

#include <cstddef>
#include <cstdint>

#include "rtl_control.hpp"
#include "rtl_profile.hpp"

namespace r820t2 {

struct Band {
    uint16_t from_mhz;      /**< applies from this LO frequency (whole MHz) up to the next row */
    uint8_t r17_open_drain; /**< bits to merge into register 17 under kR17Mask */
    uint8_t r1a_mux;        /**< bits to merge into register 1a under kR1aMask */
    uint8_t r1b_filter;     /**< register 1b, written whole */
};

constexpr uint8_t kR17Mask = 0x08;
constexpr uint8_t kR1aMask = 0xc3;

constexpr Band kBands[] = {
    {0, 0x08, 0x02, 0xdf},   {50, 0x08, 0x02, 0xbe},  {55, 0x08, 0x02, 0x8b},  {60, 0x08, 0x02, 0x7b},
    {65, 0x08, 0x02, 0x69},  {70, 0x08, 0x02, 0x58},  {75, 0x00, 0x02, 0x44},  {90, 0x00, 0x02, 0x34},
    {110, 0x00, 0x02, 0x24}, {140, 0x00, 0x02, 0x14}, {180, 0x00, 0x02, 0x13}, {250, 0x00, 0x02, 0x11},
    {280, 0x00, 0x02, 0x00}, {310, 0x00, 0x41, 0x00}, {588, 0x00, 0x40, 0x00},
};
constexpr size_t kBandCount = sizeof(kBands) / sizeof(kBands[0]);

/** The LO the PLL is programmed to: the (ppm-corrected) tuner frequency plus the IF in use. */
inline uint32_t lo_hz(uint32_t tuner_hz, double if_hz)
{
    return tuner_hz + static_cast<uint32_t>(if_hz + 0.5);
}

/** The band row for an LO frequency. */
constexpr const Band &band_for_lo_hz(uint32_t lo)
{
    const uint32_t mhz = lo / 1000000u;
    size_t i = kBandCount;
    while (i > 1 && mhz < kBands[i - 1].from_mhz) --i;
    return kBands[i - 1];
}

/** Boards whose tune template needs the band registers applied: those that replay the FM-captured sequence. */
constexpr bool profile_uses_band_select(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5;
}

/** Merge a band into one tuner write of the tune template (17, 1a or 1b); any other record is left alone. */
inline void patch_record(const Band &band, RtlControlRecord &rec)
{
    if (rec.request_type != 0x40 || rec.index != 0x0610 || rec.length != 2) return;
    uint8_t &value = rec.data[1];
    switch (rec.data[0]) {
    case 0x17: value = static_cast<uint8_t>((value & ~kR17Mask) | band.r17_open_drain); break;
    case 0x1a: value = static_cast<uint8_t>((value & ~kR1aMask) | band.r1a_mux); break;
    case 0x1b: value = band.r1b_filter; break;
    default: break;
    }
}

/* ----------------------------------------------------------------------------------------------- */
/* Blog V4 (R828D) and V4L (R828S) above the 28.8 MHz upconverter                                   */
/* ----------------------------------------------------------------------------------------------- */

/* Our 2026-10-01 vendor-DLL captures (47 tunes up and down on each, plus 10 kHz scans of every edge on the V4) show both
 * boards stepping register 1b through the same rows as the table above, keyed on the LO. The V4 also toggles register 17
 * bit 3 at fixed RF points that do not move with the sample rate; the V4L does not. At and below 28.8 MHz the measured
 * upconverter route applies and is left alone. */
constexpr uint32_t kUpconverterTopHz = 28800000u;

constexpr bool v4l_uses_band_select(RtlProfileId profile, uint32_t rf_hz)
{
    return profile == RtlProfileId::BlogV4L && rf_hz > kUpconverterTopHz;
}

struct V4Regs {
    uint8_t r17, r1a, r1b;
};

/* RF (Hz) where register 17 bit 3 flips on the V4. Open-drain is set below the first edge, then alternates; the 112 and
 * 242 MHz edges are exclusive (the register still has the earlier value at exactly 112.000 and 242.000 MHz). */
constexpr uint32_t kV4OpenDrainFlipsHz[] = {85000000u, 112000001u, 172000000u, 242000001u};

/** Final 17/1a/1b of a V4 tune above 28.8 MHz; false at or below it (the HF route keeps its own values). */
inline bool v4_regs(uint32_t rf_hz, uint32_t lo, V4Regs *out)
{
    if (out == nullptr || rf_hz <= kUpconverterTopHz) return false;
    bool open_drain = true;
    for (const uint32_t flip : kV4OpenDrainFlipsHz) {
        if (rf_hz >= flip) open_drain = !open_drain;
    }
    const Band &band = band_for_lo_hz(lo);
    out->r17 = static_cast<uint8_t>(0x20 | (open_drain ? kR17Mask : 0));
    out->r1a = static_cast<uint8_t>((0x2a & ~kR1aMask) | band.r1a_mux);
    out->r1b = band.r1b_filter;
    return true;
}

}  // namespace r820t2
