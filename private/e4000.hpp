#pragma once

#include <cstdint>

namespace e4000 {
constexpr uint32_t kMinFrequencyHz = 64000000u;
constexpr uint32_t kMaxFrequencyHz = 1700000000u;
constexpr uint32_t kReferenceXtalHz = 28800000u;
constexpr uint32_t kVcoNominalMinHz = 2600000000u;
constexpr uint32_t kVcoNominalMaxHz = 3900000000u;
enum class Band : uint8_t { Vhf2 = 0, Vhf3 = 1, Uhf = 2, L = 3 };
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
    if (band == Band::Vhf2 || band == Band::Vhf3) return 0;
    static constexpr uint32_t kUhfCentersHz[] = {360000000u,380000000u,405000000u,425000000u,450000000u,475000000u,505000000u,540000000u,575000000u,615000000u,670000000u,720000000u,760000000u,840000000u,890000000u,970000000u};
    static constexpr uint32_t kLBandCentersHz[] = {1300000000u,1320000000u,1360000000u,1410000000u,1445000000u,1460000000u,1490000000u,1530000000u,1560000000u,1590000000u,1640000000u,1660000000u,1680000000u,1700000000u,1720000000u,1750000000u};
    const uint32_t *centers = band == Band::L ? kLBandCentersHz : kUhfCentersHz;
    uint8_t best=0; uint64_t best_error=UINT64_MAX;
    for(uint8_t i=0;i<16;++i){const uint64_t e=centers[i]>frequency_hz?static_cast<uint64_t>(centers[i]-frequency_hz):static_cast<uint64_t>(frequency_hz-centers[i]);if(e<best_error){best_error=e;best=i;}}
    return best;
}
inline bool make_pll_plan(uint32_t frequency_hz, uint32_t xtal_hz, PllPlan *out)
{
    if(out==nullptr)return false;*out={};
    if(frequency_hz<kMinFrequencyHz||frequency_hz>kMaxFrequencyHz||xtal_hz<16000000u||xtal_hz>32000000u)return false;
    const Band band=band_for_frequency(frequency_hz);const bool phase=band==Band::Vhf2||band==Band::Vhf3;
    static constexpr uint8_t normal[]={2,4,6,8,12,16,20,24};static constexpr uint8_t phased[]={4,8,12,16,24,32,40,48};
    const uint8_t *divs=phase?phased:normal;uint8_t code=0,div=0;uint64_t vco=0;bool nominal=false;
    for(int i=7;i>=0;--i){const uint64_t candidate=static_cast<uint64_t>(frequency_hz)*divs[i];if(candidate>=kVcoNominalMinHz&&candidate<=kVcoNominalMaxHz){code=static_cast<uint8_t>(i);div=divs[i];vco=candidate;nominal=true;break;}}
    if(!nominal){if(phase){code=7;div=phased[7];}else if(frequency_hz<=1200000000u){code=1;div=normal[1];}else{code=0;div=normal[0];}vco=static_cast<uint64_t>(frequency_hz)*div;}
    const uint64_t z=vco/xtal_hz;if(z==0||z>255)return false;const uint64_t rem=vco%xtal_hz;const uint16_t x=static_cast<uint16_t>((rem*65536ull)/xtal_hz);
    out->valid=true;out->band=band;out->divider_code=code;out->integer_divider=static_cast<uint8_t>(z);out->fractional_divider=x;out->rf_filter_code=rf_filter_code(band,frequency_hz);out->bias_register=band==Band::L?0x0c:0x0f;out->three_phase_mixing=phase;out->vco_hz=static_cast<uint32_t>(vco);out->vco_in_nominal_range=nominal;return true;
}
} // namespace e4000
