#pragma once

/**
 * Clean-room measured Blog V4 gain / bias EP0 (2026-08-12 lab).
 *
 * Evidence (lab PC, not shipped in git by default — multi‑hundred MB):
 *   bias_on_off2.pcapng  SHA-256 33CBE20B94AD9EEBD3644DCBE1843B01007A174131337EDE8A6483FCA8EB3512
 *   gain_steps.pcapng    SHA-256 E61F9C1312371BC1344D770B607BC9B58336EFBB61C0C247516344424933E3B5
 *   RF Gain ladder: 0.0 … 49.6 dB (28 steps) recorded from SDR# UI + video
 *
 * Stimulus: rtl_biast + AIRSPY SDR# (black-box only). Tables extracted from
 * USBPcap vendor control (bmRequestType 0x40). NOT copied from librtlsdr source.
 *
 * Encoding matches existing Blog V4 profile style (RtlControlRecord):
 *   IR/demod block: wValue=0x0074, wIndex=0x0610, data[0]=reg, data[1]=value
 *   SYS block:      wValue=0x30xx, wIndex=0x0210, data[0]=value
 *
 * Re-validated from decode TSV payloads: all 28 (reg05,reg07) pairs present;
 * bias ON/OFF clusters match kMeasuredV4BiasOn/Off exactly (3001 0x19 vs 0x18).
 */

#include <cstddef>
#include <cstdint>

#include "transfers_blog_v4.hpp"

/** One measured manual gain step (tenths of dB). */
struct MeasuredV4GainStep {
    int tenth_db;
    uint8_t reg05; /**< measured via 0x0074 write pair {0x05, val} */
    uint8_t reg07; /**< measured via 0x0074 write pair {0x07, val} */
};

/**
 * Full ladder from SDR# RF Gain 0.0 → 49.6 dB (lab 2026-08-12).
 * reg 0x0c stayed 0x68 across steps (written as fixed companion).
 */
constexpr MeasuredV4GainStep kMeasuredV4GainSteps[] = {
    {0, 0xf0, 0x60},    // 0.0 dB
    {9, 0xf0, 0x60},    // 0.9
    {14, 0xf0, 0x60},   // 1.4
    {27, 0xf1, 0x60},   // 2.7
    {37, 0xf1, 0x61},   // 3.7
    {77, 0xf2, 0x61},   // 7.7
    {87, 0xf2, 0x62},   // 8.7
    {125, 0xf3, 0x62},  // 12.5
    {144, 0xf3, 0x63},  // 14.4
    {157, 0xf4, 0x63},  // 15.7
    {166, 0xf4, 0x64},  // 16.6
    {197, 0xf5, 0x64},  // 19.7
    {207, 0xf5, 0x65},  // 20.7
    {229, 0xf6, 0x65},  // 22.9
    {254, 0xf6, 0x66},  // 25.4
    {280, 0xf7, 0x66},  // 28.0
    {297, 0xf7, 0x67},  // 29.7
    {328, 0xf8, 0x67},  // 32.8
    {338, 0xf8, 0x68},  // 33.8
    {364, 0xf9, 0x68},  // 36.4
    {372, 0xf9, 0x69},  // 37.2
    {386, 0xfa, 0x69},  // 38.6
    {402, 0xfa, 0x6a},  // 40.2
    {421, 0xfb, 0x6a},  // 42.1
    {434, 0xfb, 0x6b},  // 43.4
    {439, 0xfc, 0x6b},  // 43.9
    {445, 0xfc, 0x6c},  // 44.5
    {496, 0xfd, 0x6c},  // 49.6
};

constexpr size_t kMeasuredV4GainStepCount =
    sizeof(kMeasuredV4GainSteps) / sizeof(kMeasuredV4GainSteps[0]);

/** Companion reg 0x0c value observed constant across gain ladder. */
constexpr uint8_t kMeasuredV4GainReg0c = 0x68;

/**
 * Tuner AGC AUTO — measured 2026-08-26 (agc_tuner_on_off.pcapng).
 * Four post-open clusters, ON/OFF/ON/OFF. AUTO ON is a distinct 05/07/0c trio.
 * SHA-256 E131C5C642E7DFE8443D4667975871C220FFF4A237826B8423E9E38944BD3E8E
 *
 * Manual 29.7 dB ladder is 05=F7 07=67 0C=68. AUTO is the first time 0x0c=0x6B
 * appeared on this IR path (manual ladder never changed 0x0c).
 */
constexpr uint8_t kMeasuredV4TunerAgcReg05 = 0xe8;
constexpr uint8_t kMeasuredV4TunerAgcReg07 = 0x78;
constexpr uint8_t kMeasuredV4TunerAgcReg0c = 0x6b;

/**
 * RTL2832 digital AGC (SDR# "RTL AGC") — measured 2026-08-26
 * (agc_rtl_on_off.pcapng).
 * SHA-256 1E5B006179C548E62617F530D5BC292AD45900763F6BCEF357264D4FDD55D482
 * Demod page write: wValue=0x1920 (reg 0x19 | 0x20), wIndex=0x0010 (OUT).
 * Not the R828D tuner. Do not conflate with Tuner AGC AUTO.
 */
constexpr uint8_t kMeasuredV4RtlAgcReg = 0x19;
constexpr uint16_t kMeasuredV4RtlAgcWvalue = 0x1920;
constexpr uint16_t kMeasuredV4RtlAgcWindex = 0x0010;
constexpr uint8_t kMeasuredV4RtlAgcOn = 0x25;
constexpr uint8_t kMeasuredV4RtlAgcOff = 0x05;

/**
 * Bias-T SYS sequence measured from rtl_biast clusters (toggle groups).
 * ON:  3001 data 0x19   OFF: 3001 data 0x18  (LSB differs)
 * Companion writes 3004/3003/3000 match every observed toggle cluster.
 */
constexpr RtlControlRecord kMeasuredV4BiasOn[] = {
    {0x3004, 0x0210, 0x40, 1, {0x06, 0, 0, 0, 0, 0, 0, 0}},
    {0x3003, 0x0210, 0x40, 1, {0x19, 0, 0, 0, 0, 0, 0, 0}},
    {0x3001, 0x0210, 0x40, 1, {0x19, 0, 0, 0, 0, 0, 0, 0}},
    {0x3000, 0x0210, 0x40, 1, {0x20, 0, 0, 0, 0, 0, 0, 0}},
};

constexpr RtlControlRecord kMeasuredV4BiasOff[] = {
    {0x3004, 0x0210, 0x40, 1, {0x06, 0, 0, 0, 0, 0, 0, 0}},
    {0x3003, 0x0210, 0x40, 1, {0x19, 0, 0, 0, 0, 0, 0, 0}},
    {0x3001, 0x0210, 0x40, 1, {0x18, 0, 0, 0, 0, 0, 0, 0}},
    {0x3000, 0x0210, 0x40, 1, {0x20, 0, 0, 0, 0, 0, 0, 0}},
};

constexpr size_t kMeasuredV4BiasOnCount =
    sizeof(kMeasuredV4BiasOn) / sizeof(kMeasuredV4BiasOn[0]);
constexpr size_t kMeasuredV4BiasOffCount =
    sizeof(kMeasuredV4BiasOff) / sizeof(kMeasuredV4BiasOff[0]);

/** Build 0x0074 / 0x0610 write for IR register pair (measured encoding). */
inline RtlControlRecord measured_v4_ir_reg_write(uint8_t reg, uint8_t value)
{
    RtlControlRecord rec{};
    rec.value = 0x0074;
    rec.index = 0x0610;
    rec.request_type = 0x40;
    rec.length = 2;
    rec.data[0] = reg;
    rec.data[1] = value;
    return rec;
}

/** Measured demod vendor OUT (RTL AGC uses reg 0x19 @ wIndex 0x0010). */
inline RtlControlRecord measured_v4_demod_reg_write(uint8_t reg, uint8_t value)
{
    RtlControlRecord rec{};
    rec.value = static_cast<uint16_t>((static_cast<uint16_t>(reg) << 8) | 0x20);
    rec.index = kMeasuredV4RtlAgcWindex;
    rec.request_type = 0x40;
    rec.length = 1;
    rec.data[0] = value;
    return rec;
}

/** Nearest measured step index for requested tenths-dB (clamped to table). */
inline size_t measured_v4_nearest_gain_index(int tenth_db)
{
    size_t best = 0;
    int best_err = 100000;
    for (size_t i = 0; i < kMeasuredV4GainStepCount; ++i) {
        const int err = tenth_db - kMeasuredV4GainSteps[i].tenth_db;
        const int aerr = err < 0 ? -err : err;
        if (aerr < best_err) {
            best_err = aerr;
            best = i;
        }
    }
    return best;
}
