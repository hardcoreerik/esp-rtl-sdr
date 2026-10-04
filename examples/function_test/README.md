# function_test

Automated hardware function-test firmware for `esp_rtl_sdr`. Plug in dongles, flash, get a pass/fail
report. It replaces typing the Gates from `multi_rtlsdr_test` and watching the console.

It uses the public API only, so the same run works before and after any driver change.

**Status:** the report core and runner are host-tested in CI, and the firmware compiles in the
`idf-p4-build` job. The firmware has **not yet been run on hardware**. The first run on the OrcNode is
the acceptance test for the tests themselves; expect to adjust a threshold or an assumption.

## Hardware

Same as [`multi_rtlsdr_test`](../multi_rtlsdr_test/README.md): Waveshare ESP32-P4 dev kit, DEVICE
jumper, dongles in USB-A ports 2-4 (hub), or one dongle on port 1 with the HOST jumper. Tab5 works
with one dongle. Keep nothing powered on the antenna port. An FM antenna is useful but not required.

## Build and run

```bash
cd examples/function_test
idf.py -B build -D SDKCONFIG=build/sdkconfig set-target esp32p4
idf.py -B build -D SDKCONFIG=build/sdkconfig menuconfig   # "esp_rtl_sdr function test"
idf.py -B build -D SDKCONFIG=build/sdkconfig build flash -p PORT

# capture a full run and judge it (resets the board first)
python3 ../../tests/scripts/function_test_runner.py --port PORT \
    --require-devices 3 --json report.json
```

The firmware runs once after boot and again each time you press Enter on the console. A saved console
log can be judged offline: `function_test_runner.py --log console.txt`.

Exit code: `0` pass, `1` a test failed or a requirement was not met, `2` no complete run captured.
`--require-devices N` fails the run if fewer than N dongles were found. `--fail-on-skip` treats any
SKIP as a failure.

## What it tests

| Test | Scope | What is checked |
|---|---|---|
| `env.version` | global | version macro and string |
| `policy.rates` | global | rate window edges (225000/225001, 300000/300001, 900000/900001, 3.2M) |
| `api.null_safety` | global | NULL handles return errors, never crash; `uninstall(NULL)` is OK |
| `env.enumerate` | global | dongles present, distinct USB addresses, bus device count |
| `dev.identity` | per dongle | profile known, high-speed, VID/PID, logical index |
| `dev.caps` | per dongle | required capability bits present, forbidden bits absent |
| `contract.idle` | per dongle | retune while idle, store/readback of freq, rate, ppm, bad-input rejection |
| `contract.streaming` | per dongle | second start BUSY, rate change BUSY, read() in CALLBACK mode, idempotent stop |
| `lifecycle.cycles` | per dongle | N start/stop cycles with data each time, heap does not leak |
| `stream.basic` | per dongle | IQ callbacks, bytes, IQ not constant, metrics frequency |
| `stream.rate_sweep` | per dongle | 250k to 2.4M effective rate within 5%; 3.2M must start (drops expected) |
| `stream.integrity` | per dongle | soak window: rate, IQ sequence gaps, USB errors, slot starve, overruns |
| `retune.sweep` | per dongle | FM/VHF/UHF/L-band points plus HF route edges; out-of-range refused; stream undisturbed |
| `controls.gain` | per dongle | ladder length, first/mid/last step, AUTO and MANUAL, stream keeps flowing |
| `controls.rtl_agc` | per dongle | on/off accepted, readback, stream keeps flowing |
| `controls.bandwidth` | per dongle | every listed width requested and applied |
| `controls.bias_tee` | per dongle | on/off (off by default: `CONFIG_FT_ALLOW_BIAS`) |
| `stream.read_sync` | per dongle | reinstalls in READ mode, `read()` returns data, no IQ callbacks |
| `health.report` | per dongle | health is known, not USB-starving, not app-too-slow |
| `passport.probe` | per dongle | rate passport (off by default: slow) |
| `multi.concurrent` | 2+ dongles | all stream at once; rate, gaps, errors per dongle; hub claim count |
| `multi.retune_isolation` | 2+ dongles | retune one dongle while the others keep streaming (Gate 6) |
| `multi.stop_isolation` | 2+ dongles | stop one dongle, others unaffected, restart works |
| `hotplug.unplug_replug` | operator | unplug and replug on prompt, recovers and streams (`CONFIG_FT_INTERACTIVE`) |

## Rules the suite follows

- **No silent passes.** A test that cannot run reports `SKIP` with the reason. A run with zero passes
  is not OK. The runner cross-checks the firmware's own tally against the lines it received, so lost
  serial lines are reported instead of hidden.
- **Expectations are data.** Per-profile expectations live in `main/ft_expect.hpp`. A profile with no
  row is reported as `SKIP` ("new dongle: add a row"). `tests/host/test_function_test.cpp` checks the
  table against the driver's own profile code, so the table and the driver cannot drift apart.
- **Async setters** (gain, AGC, bandwidth, bias) are judged by: request accepted, readback matches the
  request, the stream keeps flowing, no `EVT_ERROR`, state not FAULT. The driver does not read tuner
  registers back, so this is the strongest honest check. It does not prove the tuner did the right
  thing; RF checks need a known stimulus and are not in the suite yet.
- **Each test leaves the dongle IDLE.**

## Report format

One JSON object per line, mixed with normal ESP log output:

```json
{"ft":"begin","version":"0.9.3","sha":"abc123def456-dirty","board":"esp32p4"}
{"ft":"result","test":"stream.integrity","dev":0,"profile":"blog_v4_r828d","status":"PASS","ms":11800,"detail":"10s @2399812 S/s, queue_hw=3"}
{"ft":"summary","pass":41,"fail":0,"skip":3,"devices":3,"ok":true}
```

`dev` is `-1` for tests not tied to one dongle. `sha` ends in `-dirty` when built from an uncommitted tree.
Commit `report.json` under `docs/` with a release or a driver change to record the result.

## Adding a dongle

1. Add its profile to the driver.
2. Add a row to `kExpect` in `main/ft_expect.hpp` (required and forbidden caps, gain ladder length,
   RF range). The host test fails until the row matches the driver.
3. Run the suite with the dongle plugged in. Fix or document every FAIL and SKIP.

## Not covered yet

- RF correctness (tone offset, signal level, gain steps in dB) needs a stimulus.
- Absolute timing of async setters; the suite only checks that they complete.
- Long soaks; raise `CONFIG_FT_SOAK_SECONDS` or use [`docs/SOAK.md`](../../docs/SOAK.md).
