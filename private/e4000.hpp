#pragma once

#include <cstddef>
#include <cstdint>

namespace e4000 {

constexpr uint32_t kMinFrequencyHz = 64000000u;
constexpr uint32_t kMaxFrequencyHz = 1700000000u;
constexpr uint32_t kReferenceXtalHz = 28800000u;
constexpr uint32_t kVcoNominalMinHz = 2600000000u;
constexpr uint32_t kVcoNominalMaxHz = 3900000000u;

struct GainStep { int tenth_db; uint8_t lna; uint8_t mixer; };
// First-party PC gain API/USB capture, 2026-10-31. Nominal requests, not
// calibrated analog gain; 29/34 dB plateau is retained in the RF report.
inline constexpr GainStep kGainSteps[] = {
    {-10,0,0}, {15,1,0}, {40,4,0}, {65,5,0}, {90,6,0}, {115,7,0},
    {140,8,0}, {165,9,0}, {190,10,0}, {215,11,0}, {240,12,0},
    {290,13,0}, {340,14,0}, {420,14,1},
};
constexpr size_t kGainStepCount = sizeof(kGainSteps)/sizeof(kGainSteps[0]);
inline size_t nearest_gain_index(int requested)
{
    size_t best=0;
    int64_t error=INT64_MAX;
    for (size_t i=0;i<kGainStepCount;++i) {
        const int64_t delta=static_cast<int64_t>(requested)-kGainSteps[i].tenth_db;
        const int64_t distance=delta<0 ? -delta : delta;
        if (distance<error) { error=distance; best=i; }
    }
    return best;
}

enum class Band : uint8_t {
    Vhf2 = 0,
    Vhf3 = 1,
    Uhf = 2,
    L = 3,
};

struct OffsetPlan {
    uint32_t tuner_hz = 0;
    uint32_t offset_hz = 0;
    uint32_t nco_word = 0;
    uint8_t filter2 = 0xec;
    uint8_t filter3 = 0x1f;
};

template <size_t N>
inline uint8_t closest_filter(uint32_t target_hz, const uint32_t (&corners_hz)[N])
{
    uint8_t best = 0;
    uint32_t error = UINT32_MAX;
    for (uint8_t i = 0; i < N; ++i) {
        const uint32_t d = target_hz > corners_hz[i]
            ? target_hz - corners_hz[i] : corners_hz[i] - target_hz;
        if (d < error) { best = i; error = d; }
    }
    return best;
}

// First-party PC captures: LO = RF - 0.85*Fs, signed demod NCO = -0.85*Fs.
// Filter encodings follow E4000 datasheet 4v0 tables 22-24; selection is
// cross-checked against the 16-rate first-party capture. Mixer code 0xc uses
// a 3.3 MHz selection target to reproduce the observed 1.92 MS/s choice
// (the datasheet labels it 3.0 MHz). This is a selection policy, not a measured
// analog corner; hardware response still needs qualification.
// Disabled/edge-suspended mode retains the existing initialization filters.
inline bool make_offset_plan(uint32_t rf_hz, uint32_t rate_sps, bool enabled,
                             OffsetPlan *out)
{
    if (!out) return false;
    *out = {};
    if (rf_hz < kMinFrequencyHz || rf_hz > kMaxFrequencyHz ||
        rate_sps < 225001u || rate_sps > 3200000u ||
        (rate_sps > 300000u && rate_sps <= 900000u)) return false;
    out->tuner_hz = rf_hz;
    const uint32_t offset = static_cast<uint32_t>(static_cast<uint64_t>(rate_sps)*85u/100u);
    if (!enabled || rf_hz - kMinFrequencyHz < offset) return true;
    out->offset_hz = offset;
    out->tuner_hz -= offset;
    out->nco_word = (0u - static_cast<uint32_t>(
        (static_cast<uint64_t>(offset) << 22) / kReferenceXtalHz)) & 0x3fffffu;
    const uint32_t filter_hz = offset*2u;
    static constexpr uint32_t mixer[] = {
        27000000,27000000,27000000,27000000,27000000,27000000,27000000,27000000,
        4600000,4200000,3800000,3400000,3300000,2700000,2300000,1900000};
    static constexpr uint32_t rc[] = {
        21400000,21000000,17600000,14700000,12400000,10600000,9000000,7700000,
        6400000,5300000,4400000,3400000,2600000,1800000,1200000,1000000};
    static constexpr uint32_t channel[] = {
        5500000,5300000,5000000,4800000,4600000,4400000,4300000,4100000,
        3900000,3800000,3700000,3600000,3400000,3300000,3200000,3100000,
        3000000,2950000,2900000,2800000,2750000,2700000,2600000,2550000,
        2500000,2450000,2400000,2300000,2280000,2240000,2200000,2150000};
    out->filter2 = static_cast<uint8_t>((closest_filter(filter_hz, mixer)<<4) |
                                      closest_filter(filter_hz, rc));
    out->filter3 = closest_filter(filter_hz, channel);
    return true;
}

constexpr bool allows_sample_rate_record(size_t index, size_t sample_rate_first)
{
    // The captured shared slice starts with two R828D gain writes. They are
    // tuner-specific, not RTL2832U sample-rate programming.
    return index != sample_rate_first && index != sample_rate_first + 1;
}

struct PllPlan {
    bool valid = false;
    Band band = Band::Uhf;
    uint8_t divider_code = 0;
    uint8_t integer_divider = 0;
    uint16_t fractional_divider = 0;
    uint8_t rf_filter_code = 0;
    uint8_t bias_register = 0x03;
    bool three_phase_mixing = false;
    uint64_t vco_hz = 0;
    bool vco_in_nominal_range = false;
};

constexpr Band band_for_frequency(uint32_t frequency_hz)
{
    if (frequency_hz < 140000000u) return Band::Vhf2;
    if (frequency_hz < 350000000u) return Band::Vhf3;
    if (frequency_hz < 1135000000u) return Band::Uhf;
    return Band::L;
}

inline uint8_t rf_filter_code(Band band, uint32_t frequency_hz)
{
    if (band == Band::Vhf2 || band == Band::Vhf3) {
        return 0; // The VHF bands use the programmable low-pass path.
    }

    static constexpr uint32_t kUhfCentersHz[] = {
        360000000u, 380000000u, 405000000u, 425000000u,
        450000000u, 475000000u, 505000000u, 540000000u,
        575000000u, 615000000u, 670000000u, 720000000u,
        760000000u, 840000000u, 890000000u, 970000000u,
    };
    static constexpr uint32_t kLBandCentersHz[] = {
        1300000000u, 1320000000u, 1360000000u, 1410000000u,
        1445000000u, 1460000000u, 1490000000u, 1530000000u,
        1560000000u, 1590000000u, 1640000000u, 1660000000u,
        1680000000u, 1700000000u, 1720000000u, 1750000000u,
    };

    const uint32_t *centers = band == Band::L ? kLBandCentersHz : kUhfCentersHz;
    uint8_t best = 0;
    uint64_t best_error = ~static_cast<uint64_t>(0);
    for (uint8_t i = 0; i < 16; ++i) {
        const uint64_t error = centers[i] > frequency_hz
            ? static_cast<uint64_t>(centers[i] - frequency_hz)
            : static_cast<uint64_t>(frequency_hz - centers[i]);
        if (error < best_error) {
            best_error = error;
            best = i;
        }
    }
    return best;
}

/*
 * Build the E4000 fractional-N PLL plan from the public datasheet equations.
 * The nominal 2.6–3.9 GHz VCO window does not cover every RF edge with an
 * integer output-divider choice. Upper-band fallback choices are marked
 * non-nominal, logged at runtime, and still require physical validation.
 */
inline bool make_pll_plan(uint32_t frequency_hz, uint32_t xtal_hz, PllPlan *out)
{
    if (out == nullptr) {
        return false;
    }
    *out = {};
    if (frequency_hz < kMinFrequencyHz || frequency_hz > kMaxFrequencyHz ||
        xtal_hz < 16000000u || xtal_hz > 32000000u) {
        return false;
    }

    const Band band = band_for_frequency(frequency_hz);
    const bool three_phase = band == Band::Vhf2 || band == Band::Vhf3;
    static constexpr uint8_t kNormalDivisors[] = {2, 4, 6, 8, 12, 16, 20, 24};
    static constexpr uint8_t kThreePhaseDivisors[] = {4, 8, 12, 16, 24, 32, 40, 48};
    const uint8_t *divisors = three_phase ? kThreePhaseDivisors : kNormalDivisors;

    uint8_t divider_code = 0;
    uint8_t divider = 0;
    uint64_t vco_hz = 0;
    bool found_nominal = false;

    // Prefer the largest divider that keeps the VCO inside its nominal range.
    for (int i = 7; i >= 0; --i) {
        const uint64_t candidate = static_cast<uint64_t>(frequency_hz) * divisors[i];
        if (candidate >= kVcoNominalMinHz && candidate <= kVcoNominalMaxHz) {
            divider_code = static_cast<uint8_t>(i);
            divider = divisors[i];
            vco_hz = candidate;
            found_nominal = true;
            break;
        }
    }

    if (!found_nominal) {
        // Keep the published 64–1700 MHz tuning range available at edge points
        // where no divider falls inside the nominal VCO window. This fallback
        // is provisional; hardware tuning must confirm the selected divider.
        if (three_phase) {
            divider_code = 7;
            divider = kThreePhaseDivisors[7];
        } else if (frequency_hz <= 1200000000u) {
            divider_code = 1; // /4; the upper edge can exceed the nominal ceiling.
            divider = kNormalDivisors[1];
        } else {
            divider_code = 0; // /2; the lower edge can fall below the nominal floor.
            divider = kNormalDivisors[0];
        }
        vco_hz = static_cast<uint64_t>(frequency_hz) * divider;
    }

    const uint64_t z = vco_hz / xtal_hz;
    if (z == 0 || z > 255) {
        return false;
    }
    const uint64_t remainder = vco_hz % xtal_hz;
    const uint16_t x = static_cast<uint16_t>((remainder * 65536ull) / xtal_hz);

    out->valid = true;
    out->band = band;
    out->divider_code = divider_code;
    out->integer_divider = static_cast<uint8_t>(z);
    out->fractional_divider = x;
    out->rf_filter_code = rf_filter_code(band, frequency_hz);
    // First-party XTR controls frames 1785/2801; only bits [1:0] are writable.
    out->bias_register = band == Band::L ? 0x00 : 0x03;
    out->three_phase_mixing = three_phase;
    out->vco_hz = vco_hz;
    out->vco_in_nominal_range = found_nominal;
    return true;
}

} // namespace e4000
