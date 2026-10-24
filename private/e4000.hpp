#pragma once

#include <cstdint>

namespace e4000 {

constexpr uint32_t kMinFrequencyHz = 64000000u;
constexpr uint32_t kMaxFrequencyHz = 1700000000u;
constexpr uint32_t kReferenceXtalHz = 28800000u;
constexpr uint32_t kVcoNominalMinHz = 2600000000u;
constexpr uint32_t kVcoNominalMaxHz = 3900000000u;

enum class Band : uint8_t {
    Vhf2 = 0,
    Vhf3 = 1,
    Uhf = 2,
    L = 3,
};

struct PllPlan {
    bool valid = false;
    Band band = Band::Uhf;
    uint8_t divider_code = 0;
    uint8_t integer_divider = 0;
    uint16_t fractional_divider = 0;
    uint8_t rf_filter_code = 0;
    uint8_t bias_register = 0x0f;
    bool three_phase_mixing = false;
    uint32_t vco_hz = 0;
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
    out->bias_register = band == Band::L ? 0x0c : 0x0f;
    out->three_phase_mixing = three_phase;
    out->vco_hz = static_cast<uint32_t>(vco_hz);
    out->vco_in_nominal_range = found_nominal;
    return true;
}

} // namespace e4000
