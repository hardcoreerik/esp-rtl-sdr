# Four-dongle external-power soak — 2026-10-11

Status: **application provenance**, using the pinned esp-rtl-sdr commit below. This is an OrcSDR Headless integration run, not a standalone driver-example acceptance run.

**PASS for network-observed acquisition and scanning.** Four receivers ran at 2.56 MS/s for one hour on the user's external 5 V / 3 A supply, without a PC serial connection. No new IQ drops or USB errors were recorded. This does not establish watchdog-free operation: serial diagnostics were unavailable.

Observation: 3:16:23–4:16:24 PM Pacific; logger elapsed 3600.577 seconds; board uptime span 3600.430669 seconds; 119 snapshots; largest observation gap 30.867332 seconds; zero recorded failures. All snapshots retained correct presence, streaming state, rate, scan profile, valid IQ and stable USB identity. IQ, hop and FFT counters advanced between samples. Final stop-all returned ESP_OK.

| Receiver | Profile | IQ bytes consumed during window | Hops | FFT frames | New drops / USB errors |
|---|---|---:|---:|---:|---|
| Blog V4L | FM_REFERENCE | 14,517,174,272 | 11,229 | 344,244 | 0 / 0 |
| Blog V4 | VHF_ANOMALY | 12,630,163,456 | 12,318 | 404,485 | 0 / 0 |
| Nooelec NESDR SMArt v5 | ISM_902 | 14,974,337,024 | 12,482 | 376,232 | 0 / 0 |
| Blog V3 | ADSB_1090 | 14,989,557,760 | 12,511 | 380,122 | 0 / 0 |

Firmware SHA256: D921E9EBE26F7322BA6158947D6A8610CA38AD0EA7341A951ECCB28C9B182B7E. Candidate includes scanner yield and idle-at-boot changes; pinned driver a5dc9d8847012a630373e0b75099fdf3fa1b3697. This is separate from the historical four-hour soak on the original binary.

Evidence retained locally: scratchpad/external-power-2560/launch.json, configure.log, observe.py, stdout.log, stderr.log, run/status.json and run/snapshots-2560000.jsonl. The logger verdict was independently checked against snapshot coverage, receiver state, identities and progress at completion. No build, flash, restart, commit, push or merge occurred during this audit.

Published evidence: [result and provenance](2026-10-11-four-dongle-2560/), including result.json, provenance.json, all 119 compressed snapshots, the network-only recorder, and the application source patch relative to [OrcSDR Headless dba470d](https://github.com/hardcoreerik/OrcSDR-Headless/commit/dba470dadb884723d7c845d77a94a7888e349303). The SHA256 in provenance.json covers the uncompressed snapshot bytes. The application patch records the tested scanner-yield and idle-at-boot modifications; it is not a driver change.

Limits: no serial watchdog audit, RF/audio acceptance, browser-IQ transport test, or replug test. External power and cold-start conditions were changed together; this result does not isolate the cause of earlier enumeration instability or resolve remembered-count hub recovery.
