#pragma once

#include <stdint.h>

#include "esp_rtl_sdr.h"

/**
 * RTL2832 resampler ratio.
 *
 * ratio = (xtal << 22) / sps. The demod stores that in a 28-bit field with
 * the low two bits clear (mask 0x0ffffffc). Bit 27 of the stored field is
 * mirrored into bit 28 when the rate is realized. Hz must be computed from
 * the mirrored ratio. Demod bytes 0x9f..0xa2 must be programmed with the
 * stored field, which has bit 28 clear — the silicon supplies the mirror.
 *
 * Omitting the mirror reports every low-band rate at about 2×
 * (250000 Hz becomes 562500 Hz) and makes 900000 Hz indistinguishable from
 * 300000 Hz, because both stored fields are 0x08000000.
 */
inline uint32_t resampler_ratio_register(uint32_t sps)
{
    if (sps == 0) {
        return 0;
    }
    const uint32_t ratio = static_cast<uint32_t>(
        (static_cast<uint64_t>(ESP_RTL_SDR_XTAL_HZ) << 22) / sps);
    return ratio & 0x0ffffffcu;
}

inline uint32_t resampler_realized_ratio(uint32_t reg)
{
    return reg | ((reg & 0x08000000u) << 1);
}
