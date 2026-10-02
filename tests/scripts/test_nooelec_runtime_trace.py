#!/usr/bin/env python3
"""Compile actual driver functions and check Nooelec wire traces (Python + g++ only).

Run: python3 tests/scripts/test_nooelec_runtime_trace.py
This mocks successful USB transfers, not ESP32 timing, hardware, or the public API.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FUNCTIONS = """apply_freq_correction_hz encode_r820_pll tuner_i2c_value_for_handle
map_tuner_record_for_profile run_record run_records run_profile_demod_if_restore
run_v3_direct_tune run_v3_enter_direct run_v3_tuner_reinit run_nooelec_gain_restore run_v3_leave_direct
run_tune run_profile_tune frontend_rf_hz apply_r820t2_gain_records apply_gain_records
apply_tuner_agc_auto_records apply_rtl_agc_records run_bandwidth_program""".split()


def extract(source, name):
    # A definition has no ';' in its signature; bodies end at a column-zero brace.
    pattern = rf"^static [^;{{}}]*?\b{re.escape(name)}\([^;{{}}]*\)\s*\{{"
    matches = list(re.finditer(pattern, source, re.MULTILINE))
    if len(matches) != 1:
        raise RuntimeError(f"Expected one definition of {name}, found {len(matches)}")
    start = matches[0]
    end = re.search(r"^}\s*$", source[start.end():], re.MULTILINE)
    if end is None:
        raise RuntimeError(f"Missing column-zero closing brace for {name}")
    signature = source[start.start():start.end() - 1].strip()
    declaration = re.sub(r"\s*=\s*0(?=\s*[,)]\s*)", "", signature) + ";"
    return declaration, source[start.start():start.end() + end.end()]


HARNESS = r'''
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>
#include "rtl_profile.hpp"
#include "r820t2_band.hpp"
#include "transfers_blog_v3.hpp"
#include "transfers_blog_v4.hpp"
#include "measured_gain_bias_v4.hpp"
#include "measured_v4l_frontend.hpp"
#include "measured_tuner_bandwidth.hpp"
#include "gain_r820t2.hpp"
#define ESP_FAIL (-1)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define RTL_LOGW(...) ((void)0)
#define RTL_LOGE(...) ((void)0)
#define RTL_LOGD(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static int64_t esp_timer_get_time() { return 0; }
static void vTaskDelay(int) {}
struct esp_rtl_sdr_handle {
    RtlProfileId profile = RtlProfileId::NooelecSmartV5;
    uint32_t frequency_hz = 99100000, preferred_frequency_hz = 99100000;
    int32_t freq_correction_ppm = 0;
    bool ctrl_stall = false, bias_tee_want = false, tuner_auto_applied = false;
    uint32_t tuner_reg_known = 0;
    uint8_t tuner_reg_val[32] = {}, tuner_reg05_low_bits = 3, tuner_reg07 = 0x75;
    esp_rtl_sdr_gain_mode_t gain_mode = ESP_RTL_SDR_GAIN_MODE_AUTO;
    int gain_tenth_db = 0;
};
static std::vector<RtlControlRecord> wire;
static esp_err_t ctrl_submit(esp_rtl_sdr_handle *, uint8_t type, uint8_t,
    uint16_t value, uint16_t index, const uint8_t *data, uint8_t length, bool) {
    CHECK(length <= 8);
    RtlControlRecord r{value, index, type, length, {}};
    if (length) std::memcpy(r.data, data, length);
    wire.push_back(r);
    return ESP_OK;
}
static bool hf_direct_route(const esp_rtl_sdr_handle *h, uint32_t) {
    CHECK(h->profile == RtlProfileId::NooelecSmartV5); return false;
}
template <typename... Args>
static esp_err_t run_band_frontend(esp_rtl_sdr_handle *h, uint32_t, Args...) {
    CHECK(h->profile == RtlProfileId::NooelecSmartV5); return ESP_OK;
}
// ACTUAL_DRIVER_FUNCTIONS
static int last_tuner(uint8_t reg) {
    for (auto i = wire.rbegin(); i != wire.rend(); ++i) {
        if (i->request_type != 0x40 || i->index != 0x0610 || i->value != 0x0034 || i->length < 2) continue;
        const int offset = int(reg) - i->data[0] + 1;
        if (offset >= 1 && offset < i->length) return i->data[offset];
    }
    return -1;
}
static void gain(int r05, int r07, int r0c) {
    CHECK(last_tuner(5) == r05); CHECK(last_tuner(7) == r07); CHECK(last_tuner(12) == r0c);
}
static bool same(const RtlControlRecord &a, const RtlControlRecord &b) {
    return a.value == b.value && a.index == b.index && a.request_type == b.request_type &&
           a.length == b.length && std::memcmp(a.data, b.data, b.length) == 0;
}
static void records_seen(const RtlControlRecord *expected, size_t count) {
    for (size_t i = 0; i + count <= wire.size(); ++i) {
        size_t j = 0; while (j < count && same(wire[i + j], expected[j])) ++j;
        if (j == count) { CHECK(true); return; }
    }
    CHECK(false);
}
static void if_bytes(int hi, int mid, int lo) {
    const int expected[] = {hi, mid, lo};
    for (unsigned j = 0; j < 3; ++j) {
        size_t found = wire.size();
        for (size_t i = 0; i < wire.size(); ++i)
            if (wire[i].value == ((0x19 + j) << 8 | 0x20) && wire[i].index == 0x0011 && wire[i].request_type == 0x40) found = i;
        CHECK(found + 1 < wire.size()); CHECK(wire[found].length == 1); CHECK(wire[found].data[0] == expected[j]);
        const auto &settle = wire[found + 1];
        CHECK(settle.value == 0x0120 && settle.index == 0x000a && settle.request_type == 0xc0 && settle.length == 1);
    }
}
static void q_route() {
    CHECK(wire.size() >= 16);
    const size_t base = wire.size() - 16;
    for (size_t i = 0; i < 8; ++i) CHECK(same(wire[base + i], kBlogV3DirectEnable[i]));
    CHECK(wire[base + 8].value == 0x0120 && wire[base + 8].data[0] == 0x10);
    if_bytes(0x3c, 0x71, 0xc8); // Expected exact-Hz Q NCO at 1.6 MHz, zero ppm.
    for (const auto &r : wire) {
        CHECK(r.value != 0x3001 && r.value != 0x3003 && r.value != 0x3004);
        if (r.index == 0x0610 || r.index == 0x0600) CHECK((r.value & 0xff) == 0x34);
    }
}
int main() {
    esp_rtl_sdr_handle h;
    CHECK(run_v3_tuner_reinit(&h) == ESP_OK);
    gain(0x83, 0x75, 0xf0); CHECK(last_tuner(10) == 0xd3 && last_tuner(11) == 0x6b);
    CHECK(h.tuner_reg05_low_bits == 3 && h.tuner_reg07 == 0x75);
    CHECK(run_profile_demod_if_restore(&h) == ESP_OK); if_bytes(0x38, 0x11, 0x12);
    CHECK(run_profile_tune(&h, 99100000, 0) == ESP_OK); gain(0x83, 0x75, 0xf0);
    CHECK(run_profile_tune(&h, 100100000, 99100000) == ESP_OK); CHECK(last_tuner(12) == 0xf0);
    // Each captured gain survives a native retune and a Q -> native reinit.
    for (const auto &step : kR820T2GainSteps) {
        h = {}; wire.clear(); h.gain_mode = ESP_RTL_SDR_GAIN_MODE_MANUAL; h.gain_tenth_db = step.tenth_db;
        CHECK(apply_gain_records(&h, step.tenth_db, &h.gain_tenth_db) == ESP_OK);
        CHECK(wire.size() == 3); gain(step.reg05, step.reg07, 0x68);
        CHECK(wire[0].data[0] == 5 && wire[1].data[0] == 7 && wire[2].data[0] == 12);
        CHECK(run_profile_tune(&h, 99100000, 0) == ESP_OK); gain(step.reg05, step.reg07, 0x68);
        wire.clear(); CHECK(run_profile_tune(&h, 1600000, 99100000) == ESP_OK); q_route();
        wire.clear(); CHECK(run_profile_tune(&h, 100100000, 1600000) == ESP_OK);
        gain(step.reg05, step.reg07, 0x68); if_bytes(0x38, 0x11, 0x12); CHECK(h.gain_tenth_db == step.tenth_db);
        records_seen(kBlogV3DirectDisable, std::size(kBlogV3DirectDisable));
    }
    // AUTO retains the prior stage nibbles, including after hot PLL/BW writes.
    h.gain_mode = ESP_RTL_SDR_GAIN_MODE_AUTO;
    wire.clear(); CHECK(apply_tuner_agc_auto_records(&h) == ESP_OK);
    h.tuner_auto_applied = true; CHECK(wire.size() == 3); gain(0x8f, 0x7e, 0x6b);
    CHECK(run_profile_tune(&h, 99100000, 100100000) == ESP_OK); gain(0x8f, 0x7e, 0x6b);
    for (uint32_t width : kMeasuredNativeBandwidths) {
        MeasuredTunerBandwidthPlan plan{}; CHECK(measured_tuner_bandwidth_plan(h.profile, 99100000, width, &plan));
        CHECK(run_bandwidth_program(&h, 99100000, 99100000, plan) == ESP_OK);
        CHECK(last_tuner(12) == 0x6b && last_tuner(10) == 0xc3 && last_tuner(11) == plan.reg0b);
        if_bytes(plan.if19, plan.if1a, plan.if1b);
        if (width == 0) if_bytes(0x3b, 0xf7, 0x78);
    }
    wire.clear(); CHECK(run_profile_tune(&h, 1600000, 99100000) == ESP_OK); q_route();
    wire.clear(); CHECK(run_profile_tune(&h, 99100000, 1600000) == ESP_OK);
    gain(0x83, 0x75, 0x6b); CHECK(h.tuner_reg05_low_bits == 3 && h.tuner_reg07 == 0x75);
    for (bool enabled : {false, true}) {
        wire.clear(); CHECK(apply_rtl_agc_records(&h, enabled) == ESP_OK); CHECK(wire.size() == 2);
        CHECK(wire[0].value == 0x1920 && wire[0].index == 0x0010 && wire[0].request_type == 0x40);
        CHECK(wire[0].length == 1 && wire[0].data[0] == (enabled ? 0x25 : 0x05));
        CHECK(wire[1].value == 0x0120 && wire[1].index == 0x000a && wire[1].request_type == 0xc0 && wire[1].length == 1);
    }
    // The tuner reinit must not write gain itself: the I2C repeater is only switched back on after
    // it, and a gain write inside it is STALLed on hardware, so every start after a manual gain failed.
    // The restore runs separately, once the repeater is on.
    h = {}; wire.clear(); h.gain_mode = ESP_RTL_SDR_GAIN_MODE_MANUAL; h.gain_tenth_db = 496;
    CHECK(run_v3_tuner_reinit(&h) == ESP_OK); gain(0x83, 0x75, 0xf0);
    wire.clear(); CHECK(run_nooelec_gain_restore(&h) == ESP_OK); gain(0x9f, 0x6e, 0x68);
    // A Q retune can cancel queued MANUAL while the old AUTO-applied flag remains.
    h.gain_mode = ESP_RTL_SDR_GAIN_MODE_MANUAL; h.gain_tenth_db = 496;
    h.tuner_auto_applied = true;
    wire.clear(); CHECK(run_profile_tune(&h, 1600000, 99100000) == ESP_OK); q_route();
    wire.clear(); CHECK(run_profile_tune(&h, 99100000, 1600000) == ESP_OK);
    gain(0x9f, 0x6e, 0x68); CHECK(!h.tuner_auto_applied);
    // R820T2 band select: the final 17/1a/1b of a native tune follow the LO (RF + 3.57 MHz), matching the registers
    // our own PC captures show the vendor DLL leaving behind (2026-10-01 band sweep, Nooelec SMArt v5).
    struct BandCase { uint32_t rf; int r17, r1a, r1b; };
    static const BandCase kBandCases[] = {
        {46130000, 0x28, 0x2a, 0xdf},   {46730000, 0x28, 0x2a, 0xbe},   {71730000, 0x20, 0x2a, 0x44},
        {96130000, 0x20, 0x2a, 0x34},   {306730000, 0x20, 0x69, 0x00},  {433920000, 0x20, 0x69, 0x00},
        {584730000, 0x20, 0x68, 0x00},  {915000000, 0x20, 0x68, 0x00},  {1090000000, 0x20, 0x68, 0x00},
    };
    for (const auto &c : kBandCases) {
        h = {}; wire.clear();
        CHECK(run_profile_tune(&h, c.rf, 0) == ESP_OK);
        CHECK(last_tuner(0x17) == c.r17); CHECK(last_tuner(0x1a) == c.r1a); CHECK(last_tuner(0x1b) == c.r1b);
    }
    // The mid-tune PLL write to 1a (0x22) keeps its autotune bit and only takes the mux bits: 0x22 at 433.92 MHz becomes 0x61.
    h = {}; wire.clear();
    CHECK(run_profile_tune(&h, 433920000, 0) == ESP_OK);
    bool pll_1a_seen = false;
    for (const auto &r : wire)
        if (r.request_type == 0x40 && r.index == 0x0610 && r.length == 2 && r.data[0] == 0x1a && r.data[1] == 0x61)
            pll_1a_seen = true;
    CHECK(pll_1a_seen);
    std::printf("RESULT nooelec_runtime_trace passed=%u failed=0\n", checks);
}
'''


def main():
    source = (ROOT / "src/esp_rtl_sdr.cpp").read_text(encoding="utf-8")
    functions = [extract(source, name) for name in FUNCTIONS]
    code = HARNESS.replace("// ACTUAL_DRIVER_FUNCTIONS", "\n".join(
        declaration for declaration, _ in functions) + "\n" + "\n".join(
        definition for _, definition in functions))
    with tempfile.TemporaryDirectory(prefix="nooelec-runtime-") as folder:
        cpp, binary = Path(folder) / "trace.cpp", Path(folder) / "trace"
        cpp.write_text(code, encoding="utf-8")
        subprocess.run([os.environ.get("CXX", "g++"), "-std=c++17", "-O0",
                        "-I", str(ROOT / "tests/host/stubs"), "-I", str(ROOT / "include"),
                        "-I", str(ROOT / "private"), str(cpp),
                        str(ROOT / "src/esp_rtl_sdr_policy.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
