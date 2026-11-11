# Receiver profile documentation alignment — 2026-11-10

## Scope and baseline

**Documentation-only change.** Repository: [hardcoreerik/esp-rtl-sdr](https://github.com/hardcoreerik/esp-rtl-sdr). Base: default branch `master` at commit [`b3cc572`](https://github.com/hardcoreerik/esp-rtl-sdr/commit/b3cc57217c9cf68d5454080fc20fe5ae5e8403bf), whose merged documentation update is dated 2026-11-10. Working branch: `docs/receiver-profile-alignment`. Release truth is driver **v0.9.6**, published 2026-11-09; version header and `idf_component.yml` both declare **0.9.6**. [PROJECT_TRUTH.md](../../PROJECT_TRUTH.md), snapshot 2026-11-09, takes precedence over earlier test-report status labels.

This alignment reads the existing evidence; **no physical device was attached, flashed, tested, or re-soaked for this change**. OrcSDR was consulted as a read-only integration/receiver-support source, not modified. Historical versions, candidate firmware and individual observation dates are retained rather than retroactively converted into 0.9.6 hardware acceptance.

## 1. Profiles discovered in current source

The active `RtlProfileId` enumeration in [`private/rtl_profile.hpp`](../../private/rtl_profile.hpp) contains five named device profiles plus `Unknown`. The [public enum](../../include/esp_rtl_sdr.h) corresponds to the same five. The driver is **not** a universal `0bda:2838` tuner driver.

| Source profile ID | Board / tuner | Documentation | Implemented and hardware-evidence boundary |
|---|---|---|---|
| `blog_v4_r828d` | RTL-SDR Blog V4 / R828D | [BLOG_V4_PROFILE.md](../BLOG_V4_PROFILE.md) | Implemented; Tab5 FM/bandwidth and V4 HF FT8 reception documented; full analog calibration outstanding |
| `blog_v4l_r828s` | RTL-SDR Blog V4 Lite / R828S | [BLOG_V4L_PROFILE.md](../BLOG_V4L_PROFILE.md) | Implemented; Tab5 IQ, bandwidth, hotplug, FT8 and PC HF/AM evidence; individual frequency-edge calibration open |
| `blog_v3_r820t2` | Blog V3 family / R820T2 or R860 | [BLOG_V3_PROFILE.md](../BLOG_V3_PROFILE.md) | Implemented; specific **V3c** hardware RF/cold/hotplug acceptance; older revisions not automatically accepted |
| `nooelec_smart_v5_r820t2` | Nooelec NESDR SMArt V5 / R820 family | [NOOELEC_V5_PROFILE.md](../NOOELEC_V5_PROFILE.md) | Implemented, standalone Tab5 hardware-tested and fully supported in OrcSDR; FT8 accepted; uncalibrated band edges |
| `e4000_experimental` | Nooelec XTR / Elonics E4000 | [E4000_PROFILE.md](../E4000_PROFILE.md) | **Implemented despite the retained historical ID**; initial Tab5 FM/RDS, streamed IQ and in-range OrcSDR operation; long soak/full-band acceptance open |

There is no separately implemented profile for arbitrary generic RTL2832U, unsupported tuner families, or unverified physical V3 revisions. Do not mistake one driver enum covering multiple hardware revisions for model-by-model qualification.

## 2. Files created and modified

**Created four new standalone ten-section engineering references:** `docs/BLOG_V4_PROFILE.md`, `docs/BLOG_V4L_PROFILE.md`, `docs/BLOG_V3_PROFILE.md`, `docs/NOOELEC_V5_PROFILE.md`.

**Substantially expanded while preserving history:** `docs/E4000_PROFILE.md`; its existing narrative remains in an annotated, explicitly historical appendix, while current implementation/acceptance is presented first.

**Aligned:** `docs/PROFILES.md` (active matrix and five direct links, corrected controls); `docs/README.md` (profile links and this report); root `README.md` (supported-device links and reference wording); root `PROJECT_TRUTH.md` (five links, stale capability claims and 0.9.4–0.9.6 version chronology).

**Created this report:** `docs/reviews/RECEIVER_PROFILE_ALIGNMENT.md`.

All ten changed files are Markdown. **No firmware source, tuner register tables, USB control sequence, public API, dependencies, release tags, CI workflow, build configuration, OrcSDR code or hardware fixtures were modified.**

## 3. Documentation conflicts corrected

| Previous ambiguity / drift | Correction and authority |
|---|---|
| “E4000 does not expose RTL AGC” in current-facing truth | E4000 **does expose** `CAP_RTL_AGC` in the device mask; introduced in **v0.9.5**, distinct from tuner AUTO. [Registry](../../private/rtl_profile.hpp), [PR #65](https://github.com/hardcoreerik/esp-rtl-sdr/pull/65), [v0.9.6 release](../releases/v0.9.6.md). |
| V3 prose denied tuner AUTO, RTL AGC and bias-tee capability | Present registry exposes `GAIN_AUTO`, `RTL_AGC` and `BIAS_TEE`; new text states these are API capabilities, **not electrical acceptance on every V3 board**. The driver explicitly warns about ambiguous R820-family bias hardware. [Registry](../../private/rtl_profile.hpp). |
| V4 described as having the “full library set” | Device-specific V4 mask deliberately excludes Q-direct `DIRECT_SAMPLING` and E4000 `OFFSET_TUNING`. Source capabilities, not the library-wide union, govern consumer apps. |
| E4000 bridge I2C identifier written as bare `0x64` | Distinguished seven-bit tuner address `0x64` from encoded RTL2832U control `wValue=0x00c8`, supported by XTR PC frame traces and source. |
| Nooelec V5 once called provisional or pending initial hardware | Now described as **implemented**, standalone Tab5 hardware-tested and **fully supported in OrcSDR** at that project's dated snapshot. Physical before/after IF-offset comparison, complete gain calibration and broad soak remain open. |
| Confusion between V3 Q-direct and V4/V4L upconversion | Separate routes, hardware/filter configuration, numeric boundaries and unsupported controls explained in each profile. E4000 has neither native HF capability. |
| E4000 compatibility regarded as an unimplemented application feature | Linked the original [OrcSDR issue #179](https://github.com/hardcoreerik/OrcSDR/issues/179), new driver profile, October 31 receiver evidence and in-range integration policy. A closed user request does not prove broad RF acceptance. |
| Historic version/candidate language could be mistaken for current release | `PROJECT_TRUTH.md` version chronology extended through 0.9.6; each page separates source implementation, host/PC controls, Tab5 test image and OrcSDR release pin. Historical E4000 language retained with an explicit warning. |
| README/profile navigation omitted independent technical references | Central quick matrix and root/docs indexes now link all five full profile references. Main README remains concise. |

## 4. First-party validation and provenance reconciled

| Receiver | Dated RF / hardware record | Source and remaining boundary |
|---|---|---|
| V4 | **2026-09-28** Tab5 96.1 MHz bandwidth transitions and **2026-10-09** OrcSDR 7.074 MHz FT8; **2026-10-01** A/B and replug | [live bandwidth](../captures/v3c_live_bandwidth_2026-09-28.md), [Tab5 rate/hotplug](../captures/tab5_rate_matrix_2026-10-01/README.md), [OrcSDR status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) |
| V4L | **2026-09-25** PC AM1280 reception/reference captures; **2026-09-28** live Tab5 bandwidth; **2026-10-01** replug; **2026-10-09** FT8 | [V4L PC](../captures/V4L_PC_ROUTE_2026-09-25.md), [Tab5 campaign](../captures/tab5_rate_matrix_2026-10-01/README.md) |
| V3c | **2026-09-11/12** FM 96.1/99.1 and RDS, cold/hot; **2026-09-14** Q-route PC captures; **2026-09-28** bandwidth; **2026-10-09** FT8 | [capture notes](../captures/NOTES.md), [Q capture](../captures/v3c_lf_hf_direct_sampling_2026-09-14.txt), [bandwidth](../captures/v3c_live_bandwidth_2026-09-28.md) |
| Nooelec SMArt V5 | **2026-09-30** first-party PC controls; **2026-10-01** Tab5 band/replug; **2026-10-09** OrcSDR FT8; **2026-10-29** standalone Tab5 smoke | [Nooelec controls](../captures/nooelec_v5_2026-09-30.md), [Tab5 campaign](../captures/tab5_rate_matrix_2026-10-01/README.md), [acquisition report](../acquisition-reliability/IMPLEMENTATION_REPORT.md) |
| E4000/XTR | **2026-10-31** first-party PC gain/PLL plus Tab5 COM39 2.4 MS/s FM/RDS and 60.71-s bounded stream; **2026-11-01** one conducted gain-restart comparison; **2026-11-09** 0.9.6 startup recovery release | [PC controls](../captures/e4000_xtr_2026-10-31/README.md), [COM39](../validation/e4000-com39-2026-10-31/README.md), [gain gates](../E4000_GAIN_CONTROLS.md), [v0.9.6](../releases/v0.9.6.md) |

The full profiles distinguish PC vendor-DLL reference results from native driver results, stand-alone driver tests from OrcSDR integration, one-minute observations from long soaks, and nominal gain register settings from RF measurements. E4000's **Ham It Up Plus v2** 1.6 MHz AM reception at nominal 126.6 MHz dial is **external translated reception**, not an E4000 below-64-MHz capability or an implemented automatic converter-aware dashboard.

## 5. Known evidence gaps and maintainer review

- **Nooelec V5:** direct paired ESP32-P4 old-versus-new IF-offset RF measurement, analog gain/bandwidth calibration, 24 MHz PLL-warning/24–25 MHz transition, lower LF sensitivity and extended single-device soak are still separate.
- **V3 family:** V3c test coverage cannot be generalized to older V3 revisions; generic `0x96/0x69` R820 probe does not prove safe or controllable bias hardware. Gain accuracy and long soak still open.
- **V4/V4L:** exact RF passbands, loaded bias-tee current, edge behavior, antenna-dependent sensitivity and generalized tracked-filter gains require test campaigns. V4L-specific filter boundaries differ from R820-family examples.
- **E4000/XTR:** XTR reference-clock physical measurement, end-to-end full-band PLL/band edges, nominal gain ladder/AGC response (including PC 29/34-dB plateau), hotplug/soak under multiple power conditions and recurring USB-host crash evidence remain open. A patch-assisted Tab5 swap does not mean the patch ships in standalone driver 0.9.6.
- **Artifacts:** Some original raw USBPcap/CU8 runs are documented by hashes but stored in the local lab, not committed. [Nooelec PC campaign](../captures/nooelec_v5_2026-09-30.md) records a human-readable report/hash manifest discrepancy; this alignment preserves it instead of silently certifying provenance.
- **Integration/version:** OrcSDR's authoritative 2026-11-04 status references a driver **v0.9.5** pin. This standalone repository released **v0.9.6** on 2026-11-09. Maintainers must distinguish a source branch, OrcSDR nightly and released binary.
- **Validation limitation:** This documentation-only session did not run firmware compilation, host unit tests, CI, a real `bash` repository script, or physical hardware. Full test claims are dated source evidence only.

## 6. Documentation validation and results

Validation was performed **read-only against the GitHub source and file tree** in the editing session. Static checks should not be misrepresented as running CTest, ESP-IDF or actual hardware.

| Check | Result |
|---|---|
| Confirm default branch and release | **PASS:** `master` / current published driver version `0.9.6` |
| Compare source registry and public enum | **PASS:** exactly five implemented profile identifiers, excluding `Unknown`; each has a dedicated page |
| Check current branch docs/navigational references | **PASS:** root README, docs index, truth and central profiles link the appropriate pages |
| Resolve internal Markdown links against the repository tree | **PASS on completed branch:** **329 relative references checked** across all ten changed files; **zero unresolved internal links** against the final branch tree |
| Align driver version markers | **PASS:** public header and `idf_component.yml` both specify `0.9.6`; README/truth use this version |
| Documentation-only diff inspection | **PASS on completed branch:** GitHub compare from `b3cc572` reports **3 commits, ahead by 3, behind by 0**, changing exactly **10 Markdown files**; no implementation/build/config changes |
| Repo-provided test scripts | **NOT RUN:** contents reviewed; full checkout/command execution unavailable in this sandbox GitHub editing workflow |
| Physical hardware | **NOT RUN**, intentionally out of scope |

**Discovered maintainer checks** (not executed in this documentation session):

```bash
bash tests/scripts/check_truth_hygiene.sh
python3 tests/scripts/check_api_docs.py
bash tests/scripts/run_host_tests.sh
```

The first two are the repository's version/truth and API-document checks; the last runs the host suite and is useful independent release regression coverage but does not validate a physical RF receiver. All three should be run in normal CI or a complete local checkout before merging. The completed branch's 329 checked internal links resolved at review time; inspect the final PR diff and rerun the repository's normal CI before merging.

## 7. Merge policy and follow-ups

This branch is for **review**, not automatic merge. Maintainer should review the upgraded E4000 current/historical distinction, V3 family bias caution, the 24 MHz Nooelec transition and the OrcSDR pinned-driver-version boundary. Any new hardware acceptance should be recorded in a separate dated test report with exact device/host/build provenance, then linked from the relevant technical profile. No additional driver changes are implied by this documentation alignment.
