# esp_rtl_sdr — Testing Guide

> TheOrc-aligned: automated checks protect claims. Hardware remains the authority
> for USB/RF; host tests protect **policy** so docs cannot silently drift.

---

## Test layers

| Layer | Location | Requires | CI |
|---|---|---|---|
| **Host policy unit tests** | `tests/host/` | CMake + C++17 | **Yes** (every push/PR) |
| **Truth / version hygiene** | `.github/workflows/ci.yml` | shell | **Yes** |
| **On-device smoke** | `examples/p4_serial_smoke` | ESP-IDF, P4, optional Blog V4 | Manual / lab |
| **Rate passport** | `esp_rtl_sdr_probe_rates()` | P4 + Blog V4 | Manual / lab |
| **RF stimulus lab** | [TESTING.md](TESTING.md) | Heltec / Baofeng / Flipper | Manual |

Host tests **do not** prove USB streaming. They prove rate windows, quantize,
config validate, version/caps, and name tables — the pure policy that apps and
docs depend on.

---

## Host policy suite (run this first)

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File tests\scripts\run_host_tests.ps1
```

### Linux / macOS

```bash
chmod +x tests/scripts/run_host_tests.sh tests/scripts/check_truth_hygiene.sh
./tests/scripts/run_host_tests.sh
./tests/scripts/run_host_tests.sh --with-hygiene   # tests + version/docs gate
./tests/scripts/check_truth_hygiene.sh              # hygiene only
```

### Manual CMake

```bash
cmake -S tests/host -B tests/host/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/host/build
ctest --test-dir tests/host/build --output-on-failure
./tests/host/build/esp_rtl_sdr_host_tests   # or .exe on Windows
```

Expected: `RESULT passed=N failed=0` and exit code 0.

### What is under test

| Area | Examples |
|---|---|
| Version | string `X.Y.Z`, packed bits, `VERSION_NUMBER` |
| Capabilities | full “on” mask; GAIN/BIAS/DS/IQ_ACQUIRE **off** |
| Rates | all named macros; window edges; gap reject; quantize idempotent |
| Recommended list | non-empty; sorted; INVALID_SIZE partial fill |
| Frequency | quantize 1 kHz; min/max edges; presets |
| Config validate | xfer 512-multiple; count; timeout; core id; stream rate/freq/max_bytes |
| Names | all states; key err codes; NOT_V4 alias |
| Passport opts | defaults; MAX_ENTRIES |
| USB identity / ppm | VID/PID; ppm range constants |
| Error codes | distinct / alias checks |

Source under test: `src/esp_rtl_sdr_policy.cpp` (no FreeRTOS/USB).  
Driver body: `src/esp_rtl_sdr.cpp` (not host-linked).

---

## CI

Workflow: `.github/workflows/ci.yml`

1. **host-policy** — Ubuntu + Windows matrix; `-Werror` on Linux; `ctest` + direct run  
2. **truth-hygiene** — `tests/scripts/check_truth_hygiene.sh` (versions, required docs, CAP_GAIN/BIAS not enabled)  
3. **idf-p4-build** — `examples/p4_serial_smoke` with `idf.py set-target esp32p4` + `build` on ESP-IDF **v5.3.2** and **v5.4.1** (compile only; no flash/RF)  
4. **ci-ok** — aggregate gate  

| Green means | Does **not** mean |
|---|---|
| Policy unit tests pass | Blog V4 streams on hardware |
| Smoke app **compiles** for P4 | USB host runtime soak |
| Versions/docs consistent | Gain/bias work |

A red CI means **do not claim green** in docs or releases.

---

## On-device smoke (lab)

```text
examples/p4_serial_smoke + EXTRA_COMPONENT_DIRS → this repo
```

| Result | Meaning |
|---|---|
| Helpers pass, start → NO_DEVICE | Policy OK; no dongle |
| start → OK, read bytes > 0 | Stream path alive |
| `probe_rates` entries / best_stable | Passport learned on **this** host |

Log outcomes into PROJECT_TRUTH only with evidence labels (Hardware-verified vs Provenance).

---

## Good testing order (TheOrc habit)

1. **Host policy tests** (seconds, no hardware)  
2. Truth hygiene / version bump check  
3. On-device smoke if hardware available  
4. Passport + RF lab (Heltec / Baofeng / Flipper) for intent/health stories  
5. Update PROJECT_TRUTH if claims change  

---

## Adding tests

1. Prefer pure logic in `esp_rtl_sdr_policy.cpp` so host tests can link it.  
2. Add cases to `tests/host/test_policy.cpp`.  
3. Run `run_host_tests` before commit.  
4. Do not mark Planned features as covered by tests that only check CAP bits
   unless the path exists.

---

## What automated tests do **not** claim

- USB bulk sustainability on P4  
- Blog V4 RF front-end gain/bias  
- librtlsdr behavioral parity  
- Formal security audit  

Those stay manual / measured / Provenance until instrumented.
