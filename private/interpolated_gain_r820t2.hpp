#pragma once

/**
 * R820T2/R860 manual gain — two hardware-confirmed anchors, 27 interpolated
 * intermediate points. NOT a full measured table like kMeasuredV4GainSteps;
 * see docs/captures/NOTES.md for the full evidence trail and why the
 * intermediate points are estimated rather than independently captured.
 *
 * Anchors (real, hardware-confirmed, 2026-09-11):
 *   0.0 dB  -> reg05=0x90 reg07=0x60 (observed default/idle state on every
 *              real V3c and Blog V4 session immediately after tuner init,
 *              before any gain change is requested)
 *   49.6 dB -> reg05=0x9f reg07=0x6e (official RTL-SDR Blog driver,
 *              rtl_sdr.exe -g 49.6, single continuous session, confirmed
 *              settled and held with ZERO further register writes for
 *              4+ minutes -- the one fully unambiguous data point from
 *              this investigation)
 *
 * Intermediate 27 points: reg05/reg07 linearly interpolated by the
 * *requested dB fraction* (dB / 49.6) against those two anchors, using the
 * same 29 dB values the official driver itself reports as its supported
 * gain list (`rtl_test -t`) and that RTL-SDR Blog's own V4 table
 * (kMeasuredV4GainSteps) uses. This is an engineering estimate to get
 * manual gain control working end-to-end on real hardware while a full
 * measured table remains a follow-up (see NOTES.md "Recommended next
 * angle"). Do not describe these intermediate values as Hardware-verified.
 */

#include <cstddef>
#include <cstdint>

struct R820T2GainStep {
    int tenth_db;
    uint8_t reg05;
    uint8_t reg07;
};

constexpr R820T2GainStep kR820T2InterpolatedGainSteps[] = {
    {0, 0x90, 0x60},     // 0.0 dB  -- anchor
    {9, 0x90, 0x60},     // 0.9
    {14, 0x90, 0x60},    // 1.4
    {27, 0x91, 0x61},    // 2.7
    {37, 0x91, 0x61},    // 3.7
    {77, 0x92, 0x62},    // 7.7
    {87, 0x93, 0x62},    // 8.7
    {125, 0x94, 0x64},   // 12.5
    {144, 0x94, 0x64},   // 14.4
    {157, 0x95, 0x64},   // 15.7
    {166, 0x95, 0x65},   // 16.6
    {197, 0x96, 0x66},   // 19.7
    {207, 0x96, 0x66},   // 20.7
    {229, 0x97, 0x66},   // 22.9
    {254, 0x98, 0x67},   // 25.4
    {280, 0x98, 0x68},   // 28.0
    {297, 0x99, 0x68},   // 29.7
    {328, 0x9a, 0x69},   // 32.8
    {338, 0x9a, 0x6a},   // 33.8
    {364, 0x9b, 0x6a},   // 36.4
    {372, 0x9b, 0x6b},   // 37.2
    {386, 0x9c, 0x6b},   // 38.6
    {402, 0x9c, 0x6b},   // 40.2
    {421, 0x9d, 0x6c},   // 42.1
    {434, 0x9d, 0x6c},   // 43.4
    {439, 0x9d, 0x6c},   // 43.9
    {445, 0x9d, 0x6d},   // 44.5
    {480, 0x9f, 0x6e},   // 48.0
    {496, 0x9f, 0x6e},   // 49.6 -- anchor
};

constexpr size_t kR820T2GainStepCount =
    sizeof(kR820T2InterpolatedGainSteps) / sizeof(kR820T2InterpolatedGainSteps[0]);

inline size_t r820t2_nearest_gain_index(int tenth_db)
{
    size_t best = 0;
    int best_err = 100000;
    for (size_t i = 0; i < kR820T2GainStepCount; ++i) {
        const int err = tenth_db - kR820T2InterpolatedGainSteps[i].tenth_db;
        const int aerr = err < 0 ? -err : err;
        if (aerr < best_err) {
            best_err = aerr;
            best = i;
        }
    }
    return best;
}
