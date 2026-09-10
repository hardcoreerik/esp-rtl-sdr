#pragma once

#include "rtl_control.hpp"

/*
 * Provisional, non-invasive R820T2 identity trace.
 *
 * Register 0 is the public R820T2 chip-id checkpoint. This is intentionally
 * only a probe — not an invented initialization table. Blog V3 streaming
 * remains unavailable until a first-party V3 USB capture supplies startup.
 */
constexpr RtlControlRecord kBlogV3ProbeSelect = {
    0x0034, 0x0610, 0x40, 1, {0x00, 0, 0, 0, 0, 0, 0, 0}};
constexpr RtlControlRecord kBlogV3ProbeRead = {
    0x0034, 0x0600, 0xc0, 1, {0, 0, 0, 0, 0, 0, 0, 0}};
