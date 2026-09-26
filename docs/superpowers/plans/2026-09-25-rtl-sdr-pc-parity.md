# PC-Measured RTL-SDR Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the measured V4L HF route and PC control parity for V4L/V4/V3c to the single `esp_rtl_sdr` driver without claiming unmeasured RF behavior.

**Architecture:** Keep the existing profile dispatch, USB control-record runner, and paused EP0 sideband path. Put capture-derived board values in small private pure-data/policy headers, dispatch board-specific records in `src/esp_rtl_sdr.cpp`, and expose only capability-gated public controls. Preserve V4 triplexer and V3c direct-Q paths.

**Tech Stack:** C++17, ESP-IDF 5.5+ USB Host on ESP32-P4, existing C public API, CMake host tests, first-party USBPcap evidence.

**Spec:** `docs/superpowers/specs/2026-09-25-rtl-sdr-pc-parity-design.md`

## Global Constraints

- Source values only from `docs/captures/V4L_PC_ROUTE_2026-09-25.md`; keep nominal gain and unmeasured analog passband claims explicit.
- V4L tuner I2C `0x0034`; V4 `0x0074`; V3c HF remains direct-Q, not upconverted.
- At exactly 28.8 MHz V4L PLL is native while GPIO remains HF; above it GPIO is VHF.
- Bias OFF by default and on stop/cleanup; clear the ON preference on detach and force OFF on new attachment. BlogV3 enables only after an explicit setter call; OrcSDR warns first.
- Live bandwidth changes serialize with retune, pair filter/PLL/demod IF, roll back on error, and fault if rollback fails.
- No OrcSDR edit, device flash, push, merge, tag, or RF acceptance claim in this driver plan. Claude receives an OrcSDR prompt only after driver work is verified.

## Review Focus

1. Exactly 28.8 MHz: V4L must not add the upconverter LO, but must retain the observed HF GPIO value (Task 1 test).
2. Generic Realtek replacement after V3c bias ON: desired ON state must clear and the next attachment must start OFF (Task 3 review/test).
3. Retune plus bandwidth request in one delivery window: a PLL may never use a different IF from the demod (Task 4 test).
4. A failed live bandwidth write followed by failed rollback: stream must fault, not resume uncertain IQ (Task 4 test).
5. V3c direct-Q HF bandwidth request: return unsupported rather than presenting bypassed tuner writes as an analog filter (Task 4 test).

---

### Task 1: Pure V4L route model and profile policy

**Files:**
- Create: `private/measured_v4l_frontend.hpp` — board-only route and gain-register composition.
- Modify: `private/rtl_profile.hpp` — V4L tuner frequency calculation only; keep HF fail-closed until Task 2 wires the route.
- Test: `tests/host/test_profiles.cpp` — route boundaries and V4/V3/Nooelec isolation.

**Interfaces:**
- Produces: `MeasuredV4LFrontendPlan measured_v4l_frontend_plan(uint32_t rf_hz, bool bias_on, uint8_t reg05_low_bits)` with tuner input/filter and GPIO bytes; `rtl_profile_tuner_frequency_hz()` returns RF+28.8 MHz only for V4L RF below 28.8 MHz. `rtl_profile_supports_rf_hz()` still rejects V4L HF in this intermediate commit.
- Preserves: V4 `measured_v4_frontend_plan()`, V3 direct-Q policy, R820T2 remap, and driver-wide frequency limits.

- [ ] **Step 1: Add failing host assertions** for V4L RF 590 kHz, 1.28 MHz, 23,999,999, 24 MHz, 28,799,999, exactly 28.8 MHz, 28,800,001, 96.1 MHz, 250 MHz and 1090 MHz. Assert PLL input frequency class, observed `0x3001` GPIO (`18` through exactly 28.8 MHz, `38` above), `0x0034` tuner address, V4/V3/Nooelec unchanged, and V4L HF still fail-closed until Task 2.
- [ ] **Step 2: Run** `powershell -NoProfile -ExecutionPolicy Bypass -File tests/scripts/run_host_tests.ps1`; expect the profile target to fail on missing V4L route-plan and upconverter-frequency assertions.
- [ ] **Step 3: Implement** the V4L-only pure plan and profile policy in the files above; model exact-28.8 asymmetry explicitly rather than reusing V4's triplexer band enum.
- [ ] **Step 4: Run** the same host command; expect all existing host targets to pass. Confirm `git diff --check` and a narrow diff.
- [ ] **Step 5: Commit** only Task 1 files with a V4L route-policy message.

### Task 2: Apply V4L route and matched IF on cold start and retune

**Files:**
- Modify: `src/esp_rtl_sdr.cpp` — dispatch `run_band_frontend()` to a V4L-specific record sequence; apply after cold init and during retune; retain existing V4 and V3 behavior.
- Modify: `private/rtl_profile.hpp` — enable V4L HF frequency support and `CAP_HF_UPCONVERTER` only with the wired route.
- Modify: `private/measured_v4l_frontend.hpp` only for exact capture-derived records needed by the runner.
- Test: `tests/host/test_profiles.cpp` — expected V4L record plan and V4 isolation.

**Interfaces:**
- Consumes: Task 1 `measured_v4l_frontend_plan()` and `rtl_profile_tuner_frequency_hz()`.
- Produces: `run_v4l_frontend_before_tune(esp_rtl_sdr_handle *, uint32_t)` and `run_v4l_frontend_after_tune(esp_rtl_sdr_handle *, uint32_t)` internal helpers consuming Task 1's pure plan. The first emits the captured input-register phase, `run_tune()` programs the PLL, and the second emits the captured route/GPIO/gain phase, all on tuner I2C `0x0034`. No V4 reg-06 triplexer or bias companion, and no V3 direct sampling.

- [ ] **Step 1: Add failing plan tests** for AM1280 → FM96.1 → AM1280 and exactly 28.8 MHz. Assert pre-PLL `17/1a/1b` and post-PLL `1a/1b/GPIO/05` values from the PC report, matching IF, no V4 reg-06 triplexer or `0x0074` address, and V4L HF support/capability flipping only in this task.
- [ ] **Step 2: Run** the host test script; expect the new record-plan test to fail because the V4L runner is absent.
- [ ] **Step 3: Implement** V4L cold/hot pre-PLL and post-PLL route dispatch around `run_tune()` in `src/esp_rtl_sdr.cpp`, retaining the existing bulk-pause and I2C-repeater gates. Replace the VHF/UHF-only log; leave the transient PC IQ anomaly unmodified because its cause is unknown.
- [ ] **Step 4: Run** host tests and `git diff --check`; verify the previous V4 and V3 tests still pass. Run one ESP-IDF component/example build if the local toolchain is available; otherwise record the missing build gate.
- [ ] **Step 5: Commit** only Task 2 files with a V4L cold/hot route message.

### Task 3: Board-specific gain, tuner AUTO, RTL AGC and bias safety

**Files:**
- Modify: `private/measured_v4l_frontend.hpp` — V4L FM/HF manual and AUTO values.
- Modify: `private/rtl_profile.hpp` — capability matrix for BlogV3/V4L, excluding Nooelec/Unknown bias.
- Modify: `include/esp_rtl_sdr.h` — document that BlogV3 `CAP_BIAS_TEE` is a user-invoked control, not proof of voltage on all generic sticks.
- Modify: `src/esp_rtl_sdr.cpp` — profile-specific gain table reporting and writes, AUTO, digital AGC, bias dispatch, warning on BlogV3 enable, and bias-OFF reset on stop/detach/reselect/new attachment.
- Test: `tests/host/test_profiles.cpp` — pure value/static capability policy and `rtl_profile_clear_bias_request(bool &want)`; inspect every lifecycle call site.
- Modify: `docs/API_REFERENCE.md` and `docs/CAPABILITY_MATRIX.md` — describe profile gain/AGC, manual-enable warning, and bias reset semantics.

**Interfaces:**
- Consumes: Task 1 V4L frontend composition and existing `kR820T2GainSteps` (29 values) / V4 measured table.
- Produces: existing `esp_rtl_sdr_set_bias_tee()` works for claimed BlogV3 after an explicit ON request; `CAP_BIAS_TEE` is static for that profile. `rtl_profile_clear_bias_request(bool &want)` clears `bias_tee_want` on stop/detach/select/uninstall; the next attachment receives an explicit OFF control before streaming.

- [ ] **Step 1: Add failing assertions** for V4L FM `90/60` to `9f/6e`, AM `f0/60` to `ff/6e` including 48.0 dB; V3c 29 returned gains; V4 unchanged; V4L/V3c AUTO values; RTL AGC OFF/ON/OFF `05/25/05`; V4L/V4/BlogV3 bias GPIO `18/19`; Nooelec and Unknown negative cases. Set a boolean bias request true, call `rtl_profile_clear_bias_request()`, and assert it is false.
- [ ] **Step 2: Run** the host script; expect the new assertions to fail on current capabilities and V4-only gain enumeration.
- [ ] **Step 3: Implement** the minimal profile-dispatched records in `src/esp_rtl_sdr.cpp`; use the existing paused sideband queue, log a generic-identity warning on BlogV3 ON, clear its ON preference on stop/detach/reselect, and initialize the next claimed board OFF. Do not advertise an effective tuner AUTO control on V3c's bypassed HF path.
- [ ] **Step 4: Run** host tests, targeted component build, and `git diff --check`. Confirm no V4 or Nooelec capability regression.
- [ ] **Step 5: Commit** only Task 3 files with a gain/AGC/bias parity message.

### Task 4: Measured tuner bandwidth API and live rollback

**Files:**
- Create: `private/measured_tuner_bandwidth.hpp` — seven native-route and four HF-route measured width plans with board-specific `0a`.
- Modify: `include/esp_rtl_sdr.h` — append `ESP_RTL_SDR_CAP_TUNER_BANDWIDTH`; declare supported-width, set, and requested/applied-state queries (Hz; zero means auto).
- Modify: `src/esp_rtl_sdr.cpp` — serialize bandwidth with retune in the existing paused EP0 path, apply filter/PLL/demod IF together, rollback or fault.
- Test: `tests/host/test_profiles.cpp` — plan values, direct-Q rejection, and a fake-writer transaction test.
- Modify: `docs/API_REFERENCE.md` and `docs/CAPABILITY_MATRIX.md` — exact capability and async applied-state semantics.

**Interfaces:**
- Consumes: Task 2 V4L route/IF runner and Task 3 capability state.
- Produces: `esp_rtl_sdr_get_tuner_bandwidths(handle, uint32_t *hz, size_t max_count, size_t *out_count)`, `esp_rtl_sdr_set_tuner_bandwidth(handle, uint32_t hz)`, and `esp_rtl_sdr_get_tuner_bandwidth_state(handle, uint32_t *requested_hz, uint32_t *applied_hz)`; active requested/applied values remain distinct until EP0 succeeds. A private `rtl_bandwidth_commit(previous_plan, next_plan, writer)` returns `Applied`, `RolledBack`, or `Fault` after calling `writer(next_plan)` and, on error, `writer(previous_plan)`.

- [ ] **Step 1: Add failing tests** for FM/native 0/200/300/500/1000/1800/2400 kHz, V4/V4L HF 0/200/500/2400 kHz, V4L `0a=c4`, V4 `0a=c5`, matched IF 1.815/2.125/2.025/1.815 MHz at AM, V3c direct-Q HF `UNSUPPORTED`, and unmeasured requests rejected. Build the plan from the pending retune RF when both are queued. Fake `writer(next)` failure then `writer(previous)` success/failure and assert `RolledBack`/`Fault`.
- [ ] **Step 2: Run** the host script; expect new API/plan tests to fail under the current header and profile code.
- [ ] **Step 3: Implement** the width plan and public API, then the queued live transaction. On first write failure, try the saved applied plan; on rollback failure, set/report FAULT before IQ resumes. Do not report queued request as applied success.
- [ ] **Step 4: Run** host tests, targeted ESP-IDF build, and `git diff --check`; inspect the exact public-header and board-route diff for accidental feature claims.
- [ ] **Step 5: Commit** only Task 4 files with a tuner-bandwidth/rollback message.

### Task 5: Final driver verification and OrcSDR handoff prompt

**Files:**
- No required file edits after Task 4; preserve the PC report and approved spec/plan.
- Test: targeted host tests and one component/example build; no flash in this task.

**Interfaces:**
- Produces: exact local driver commit identity and a Claude prompt to pin it in OrcSDR, warn before enabling BlogV3 bias on a generic descriptor, and separately prove build, flash, AM audio/spectrum, gain/AGC/bandwidth, bias OFF, and three-dongle hot-swap.

- [ ] **Step 1: Re-read** the approved spec against Task 1–4 diffs and `git log --oneline origin/master..HEAD`; record any unimplemented or hardware-only gate.
- [ ] **Step 2: Run** `powershell -NoProfile -ExecutionPolicy Bypass -File tests/scripts/run_host_tests.ps1`, the available targeted ESP-IDF build, and `git diff --check`; record exact outputs, not inferred pass status.
- [ ] **Step 3: Review** the local commit range for V4/V3/Nooelec regressions and dangerous bias defaults; fix only actionable findings and rerun affected tests.
- [ ] **Step 4: Provide** the user the driver commit/hash and a Claude prompt with explicit OrcSDR pin/build/flash/device-acceptance gates. Do not push, merge, flash, or claim OrcSDR success without separate authorization/evidence.
