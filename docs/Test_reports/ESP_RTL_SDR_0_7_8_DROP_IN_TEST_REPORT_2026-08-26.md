# esp_rtl_sdr 0.7.8 Drop-In and Hardware Test Report

Date: 2026-08-26  
Repository: `https://github.com/hardcoreerik/esp-rtl-sdr.git`  
Source branch/commit: `master` / `b74bbfe6fb278c8674490ba18f2495810a23128e`  
Test branch: `codex/0-7-8-drop-in-tests`  
Test worktree: `C:\Users\hardc\Documents\Codex\2026-08-26\lets-pause-for-a-bit\esp-rtl-sdr-0-7-8-tests`

## Executive result

The host, hygiene, ESP32-P4 build, flash, and attached RTL-SDR Blog V4 smoke
layers were run. The final bounded hardware matrix passed:

```text
SMOKE OVERALL PASS passed=26 failed=0 hardware=RUN
```

The run proved that the 0.7.8 public contract is present, the Blog V4 streams
IQ, manual gain remains usable, tuner AUTO and RTL AGC requests are accepted,
IQ reads continue after transitions, and stop/uninstall succeed. USB overrun
delta was zero.

Testing also found one confirmed driver concurrency defect and one generic
smoke configuration limit:

1. `in_callback_depth` is handle-global, so a normal application-task setter
   can incorrectly receive `ESP_RTL_SDR_ERR_REENTRANT` while another task is in
   an event callback. The documented contract only forbids reentry from the
   callback itself.
2. The default auto-sized read ring could not allocate in the generic Tab5
   smoke configuration, where external PSRAM was not enabled. Explicitly sizing
   the ring to 64 KiB allowed the required reads to run. This is a drop-in
   configuration risk, not proof that PSRAM-enabled consumers will fail.

Current verdict: **L4 API/stream continuity passes with a bounded read ring;
unconditional default-config drop-in acceptance does not pass because of the
false-reentrancy defect and auto-ring allocation behavior.** L5 unchanged
consumer testing and L6 soak remain open.

## Scope and preservation

The handoff was treated as the test specification. No measured EP0 tables,
public API, driver implementation, consumer repository, or captured USB
evidence was changed. The optional CLI was not added; the one-shot matrix is
the smallest required test.

The primary network checkout and all existing modified/untracked files were
preserved. Implementation changes remain isolated in the test worktree. No
commit, push, PR, merge, or branch deletion was performed.

## Test implementation

The smoke now emits grep-friendly rows:

```text
SMOKE <name> PASS|FAIL
SMOKE OVERALL PASS|FAIL passed=<n> failed=<n> hardware=<state>
```

| Area | Live assertions |
|---|---|
| Contract | policy helpers and exact driver version 0.7.8 |
| Capabilities | GAIN, GAIN_AUTO, RTL_AGC, BIAS_TEE |
| Manual regression | 0, 297, and 400 -> 402; mode MANUAL |
| Tuner AUTO | set/get shadow, idempotence, read, manual forcing, AUTO restore |
| RTL AGC | on/off shadow, independence from tuner AUTO, read |
| Restore | MANUAL mode, gain 297, read |
| Runtime | metrics before/after, health, stop, uninstall |
| No device | bounded enumeration wait; true NO_DEVICE remains a passing skip |

The test callback ignores `EVT_IQ_BLOCK` logging so serial output cannot stall
the delivery path. The read ring is explicitly 64 KiB because the smoke reads
only at transition checkpoints and does not continuously drain IQ.

Changed test files:

- `examples/p4_serial_smoke/main/main.c`
- `examples/p4_serial_smoke/README.md`
- `examples/p4_serial_smoke/sdkconfig.defaults`
- `docs/TESTING_GUIDE.md`

The two sdkconfig defaults select the ESP32-P4 pre-v3 silicon family so the
image is flashable on the Tab5's P4 revision 1.3.

## Automated evidence

### Host policy — PASS

```text
powershell -ExecutionPolicy Bypass -File tests\scripts\run_host_tests.ps1
RESULT passed=219 failed=0
HOST_TESTS_OK
```

### Truth/version hygiene — PASS

```text
tests/scripts/check_truth_hygiene.sh
header version=0.7.8
TRUTH_HYGIENE_OK ver=0.7.8
```

### Native ESP32-P4 build — PASS

Environment: ESP-IDF 5.5.4, target `esp32p4`, pre-v3 P4 image family.

```text
idf.py build
esp_rtl_sdr_p4_smoke.bin binary size 0x4fc40 bytes
0xb03c0 bytes (69%) free
```

Final binary SHA-256:

```text
416F1A5A43EB2695F36EDDE2B0201615FC1AB0BBB8EAD1362118E7E5909D491D
```

The generated build directory was preserved and remains ignored.

## Flash and boot evidence

Port: `COM17`  
Board: ESP32-P4 revision v1.3  
Console/flash transport: USB Serial/JTAG  
Attached receiver: RTLSDRBlog Blog V4, serial `00000001`, high-speed USB

The first flash attempt safely stopped before writing because the initially
generated image required P4 revision 3.1 or newer. After selecting the pre-v3
family in `sdkconfig.defaults`, the rebuild and flash completed:

```text
Chip is ESP32-P4 (revision v1.3)
Hash of data verified.
Hard resetting via RTS pin...
```

Fresh boot then reported:

```text
ESP-IDF: v5.5.4-dirty
Min chip rev: v0.1
Max chip rev: v1.99
Chip rev: v1.3
install v0.7.8 caps=0x000fff3f xfer=6x16384
open RTLSDRBlog Blog V4 serial=00000001 hs=1
```

The application `git describe` string printed `v0.7.7-1-gb74bbfe-dirty`; the
driver's public version string and installed runtime both printed 0.7.8.

## Hardware run 1: default-style stress — FAIL, useful bug evidence

The first attached-device run used the default BOTH delivery mode, auto-sized
pull ring, and logged every event. USB enumeration completed at about 560 ms,
after the smoke had initially checked at about 160 ms. A bounded 3-second
enumeration wait was added to the harness.

With the device streaming, the run ended:

```text
SMOKE OVERALL FAIL passed=20 failed=6 hardware=RUN
```

Observed failures included:

- IQ reads returning `ESP_ERR_NO_MEM` because the auto-sized pull ring could
  not allocate in this generic configuration;
- `manual_restore_mode` receiving `ESP_RTL_SDR_ERR_REENTRANT` from the app task
  while delivery-task callbacks were active;
- large consumer-drop counts caused by an undrained pull ring and synchronous
  per-IQ serial logging.

Despite those failures, IQ bytes continued increasing and USB overrun delta
remained zero. This run is retained as negative drop-in evidence; it was not
relabelled as a pass.

## Hardware run 2: bounded matrix — PASS

The rerun kept BOTH delivery semantics but suppressed raw IQ-event logging and
used a 64 KiB pull ring. Device enumeration succeeded after the bounded wait.

```text
version_0_7_8                 PASS
cap_gain                      PASS
cap_gain_auto                 PASS
cap_rtl_agc                   PASS
cap_bias_tee                  PASS
manual_gain_0                 PASS
manual_gain_297               PASS
manual_gain_400_nearest_402   PASS
tuner_auto_set_get            PASS
tuner_auto_idempotent         PASS
tuner_auto_read               PASS, 4096 bytes
gain_forces_manual            PASS
tuner_auto_restore            PASS
rtl_agc_on                    PASS
rtl_agc_independent           PASS
rtl_agc_off                   PASS
rtl_agc_read                  PASS, 4096 bytes
manual_restore_mode           PASS
manual_restore_gain           PASS
manual_restore_read           PASS, 4096 bytes
metrics_after                 PASS
health_after                  PASS
stop                          PASS
uninstall                     PASS
SMOKE OVERALL PASS passed=26 failed=0 hardware=RUN
```

Rate passport:

```text
entries=9
best_stable_sps=3200000
```

Final matrix metrics:

```text
before_bytes=2555904
after_bytes=185779456
overrun_delta=0
consumer_drop_delta=16015360
effective_sps=2935143
last_error=ESP_OK
health.efficiency=0.918
health.overruns=0
health.consumer_drops=18518016
health.advice=app too slow
```

The consumer drops are expected from this intentionally small ring being read
only three times during a multi-second 3.2 MS/s test. They do not indicate USB
overruns; a continuously draining consumer is required for meaningful long-run
consumer-drop acceptance.

Repeated `USBH: Dev 1 EP 0 STALL` messages appeared during known device setup
sequences but were nonfatal: each stream subsequently started and produced IQ.

## Confirmed defect: false cross-task REENTRANT

The public header says lifecycle/setter reentry is forbidden *from inside the
callback*. The implementation checks only a handle-wide counter:

```text
if (h != nullptr && h->in_callback_depth > 0)
    return ESP_RTL_SDR_ERR_REENTRANT;
```

Because it does not identify the callback task, an unrelated application task
can be rejected during any concurrent callback. The first hardware run
reproduced this on `set_tuner_gain_mode(MANUAL)`. This can affect existing
default-BOTH consumers with event callbacks and is a real drop-in concurrency
bug. No driver fix was made because this task was scoped to implementing and
executing tests.

## Evidence boundary

While streaming, gain, tuner AUTO, RTL AGC, and bias setters queue EP0 work on
the delivery task. `ESP_OK` means accepted. Getters report requested software
shadow state, not physical register readback.

This report may claim API contract, CAP bits, IQ continuity, metrics, and health
around requests. It does **not** claim that the P4 read back R828D `05=E8`, tuner
`07=78`/`0C=6B`, or demod `0x19=0x25`. Physical register evidence remains the PC
USBPcap. Matching getters are not a substitute for that capture.

## Remaining gaps

| Evidence layer | Status |
|---|---|
| L0 hygiene | PASS |
| L1 host policy | PASS, 219/219 |
| L2 P4 compile/link | PASS, ESP-IDF 5.5.4 |
| Flash/boot | PASS, Tab5 P4 v1.3 |
| L3 physical no-dongle run | NOT RUN; initial zero count was enumeration timing, not absence |
| L4 Blog V4 matrix | PASS with 64 KiB ring; default-style stress FAIL retained |
| Physical register readback | NOT RUN; PC USBPcap remains separate evidence |
| L5 unchanged consumer | NOT RUN; Phase B was not authorized |
| L6 15-minute/1-hour soak | NOT RUN |

No claim of full hardware verification, production stability, or unchanged
consumer drop-in compatibility should be made until the cross-task reentrancy
bug is fixed, an unchanged consumer is exercised, and a continuously drained
soak log is completed.
