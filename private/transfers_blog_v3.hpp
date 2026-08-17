#pragma once

#include "rtl_control.hpp"

/*
 * Provisional, non-invasive R820T2 identity trace.
 *
 * The R820T2 public register description defines register 0 as the read-mode
 * chip-id checkpoint. This is intentionally only a probe, not an invented
 * initialization table. V3 streaming remains unavailable until a V3 capture
 * supplies the RTL2832U and tuner startup sequence.
 */
constexpr RtlControlRecord kBlogV3ProbeSelect = {
    0x0034, 0x0610, 0x40, 1, {0x00, 0, 0, 0, 0, 0, 0, 0}};
constexpr RtlControlRecord kBlogV3ProbeRead = {
    0x0034, 0x0600, 0xc0, 1, {0, 0, 0, 0, 0, 0, 0, 0}};
