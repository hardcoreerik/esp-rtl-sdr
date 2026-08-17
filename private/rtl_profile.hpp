#pragma once

#include <cstdint>
#include <cstring>

enum class RtlProfileId : uint8_t {
    Unknown = 0,
    BlogV3,
    BlogV4,
};

struct RtlProfileProbeResult {
    bool completed = false;
    uint8_t chip_id = 0;
};

inline bool rtl_profile_text_is(const char *actual, const char *expected)
{
    return actual != nullptr && expected != nullptr && std::strcmp(actual, expected) == 0;
}

inline RtlProfileId rtl_profile_from_descriptors(uint16_t vid, uint16_t pid,
                                                 const char *manufacturer,
                                                 const char *product)
{
    if (vid != 0x0BDA || pid != 0x2838 || !rtl_profile_text_is(manufacturer, "RTLSDRBlog")) {
        return RtlProfileId::Unknown;
    }
    if (rtl_profile_text_is(product, "Blog V4")) {
        return RtlProfileId::BlogV4;
    }
    if (rtl_profile_text_is(product, "Blog V3") ||
        rtl_profile_text_is(product, "RTL-SDR Blog V3")) {
        return RtlProfileId::BlogV3;
    }
    return RtlProfileId::Unknown;
}

inline bool rtl_profile_v3_probe_matches(const RtlProfileProbeResult &probe)
{
    /* The public R820T2 register description shows 0x96 on the wire; some
     * RTL2832U bridge paths return the bit-reversed 0x69 form. Accept only
     * those two completed reads and never treat a STALL as a match. */
    return probe.completed && (probe.chip_id == 0x96 || probe.chip_id == 0x69);
}

inline RtlProfileId rtl_profile_select(uint16_t vid, uint16_t pid, const char *manufacturer,
                                       const char *product,
                                       const RtlProfileProbeResult &v3_probe)
{
    const RtlProfileId descriptor =
        rtl_profile_from_descriptors(vid, pid, manufacturer, product);
    if (descriptor != RtlProfileId::Unknown) {
        return descriptor;
    }
    if (vid == 0x0BDA && pid == 0x2838 && rtl_profile_v3_probe_matches(v3_probe)) {
        return RtlProfileId::BlogV3;
    }
    return RtlProfileId::Unknown;
}

inline const char *rtl_profile_name(RtlProfileId profile)
{
    switch (profile) {
    case RtlProfileId::BlogV3: return "blog_v3_r820t2";
    case RtlProfileId::BlogV4: return "blog_v4_r828d";
    default: return "unknown";
    }
}
