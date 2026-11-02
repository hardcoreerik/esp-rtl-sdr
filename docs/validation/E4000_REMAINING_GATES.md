> Release 0.9.4: the historical one-line HAL workaround below is NOT shipped or recommended by this driver release. Error/CHHLTD sequencing needs independent SDK qualification. Use a separately qualified SDK; this driver does not modify it.

# E4000 test completion audit

Current audit: October 31, 2026. This is not release acceptance.
Driver branch `codex/lab-regression-20261029`; app branch
`codex/e4000-offset-20261031` in the separate OrcSDR repository.

| Requirement | Current evidence | State / next action |
|---|---|---|
| Identify real SMArt XTR descriptor and E4000 bridge address | Driver profile tests; first-party USB captures; COM39 enrollment | Passed for serial 82115932 |
| Deliver IQ after initialization | FIFO release trace regression; COM39 physical streaming | Passed for tested fixture |
| Offset policy, sign and filters | 16-rate PC controls; profile and actual-function trace tests | Passed host/reference checks; E4000 remains experimental |
| Improve center artifact without moving test tone | Six hash-verified COM39 captures, offset OFF/ON/OFF at 2.4 MS/s | Passed: 20.14 dB center-peak reduction, -1.17 dB tone-level change |
| Record default and custom-rate diagnostic IQ | Failed zero-byte 960 kS/s run preserved; narrow app routing fix; successful 960 kS/s and 3.2 MS/s captures | Passed tested rates; latest app image differs from initial matrix only by recorder routing fix |
| Restart/reconnect and uninterrupted hotplug | App evidence commits 0063b8c4 and 8290aa04 | Passed one cycle each on pre-recorder-fix image; not long-run reliability proof |
| Driver recovery regression | `cmake --build tests/host/build --config Debug --target esp_rtl_sdr_recovery_tests`; `ctest --test-dir tests/host/build -C Debug -R '^recovery$' --output-on-failure` | Passed, 1/1 test; mocks do not prove SDK ISR recovery |
| USB HAL error-before-halt panic | Matching saved crash; official upstream patch; 202 actual-function host checks | Host reproduction/correction passed; isolated patched firmware build and physical recovery pending approval |
| FM audio / RDS / translated AM with offset candidate | Earlier non-offset firmware reception is documented, but does not prove current candidate | Pending antenna change and current-image RF evidence |
| Existing Blog V4 and Nooelec SMArt v5 hardware regression | Legacy host traces pass; older physical results exist | Latest-driver hardware run still needed; COM17 must not be flashed |
| E4000 gain / AGC controls | PC gain capture exists; manual gain and tuner AUTO implemented; fixed-gain restart pair improved to 0.088 dB | RTL AGC unsupported; broader gain/AUTO physical acceptance and console reliability remain open |
| Automated reproducible evidence | PC scripts, hashes, app serial capture, rate verifier, screenshots and scoped host tests | Present for completed tests; physical hardware tests require fixture setup |
| Publish / merge / release | New work remains local on branches; app manifest references local Git | Not landed; no release tag. Replace local dependency with published exact revision before distributable build |

Evidence locations:

- Driver PC reference and tone results: `captures/e4000_xtr_2026-10-31/offset/`
  relative to the repository `docs` directory.
- Driver crash diagnosis: `e4000-com39-2026-10-31/CRASH.md` and
  `usb-hal-host-check.json`.
- OrcSDR: `docs/evidence/e4000-offset-2026-10-31/`, including image/source
  provenance, raw-IQ hashes, results, PNGs and sanitized lifecycle logs.
- Raw captures, full serial logs, recovery images and crash memory remain
  local in ignored build/lab folders. Do not publish raw crash memory.

Pending physical input: replace the tinySA RF fixture with the antenna that
previously received FM/RDS, recording its identity. Pending approval: one
isolated HAL-patched firmware build/flash/recovery loop. TinySA was last
acknowledged OFF and paused. No additional flash was performed for this audit.

Release 0.9.4: historical audit above is not current release acceptance. Experimental E4000 scope and current gain gates are documented in ../E4000_GAIN_CONTROLS.md. External SDK HAL workaround is excluded from release support.
