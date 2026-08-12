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
chmod +x tests/scripts/run_host_tests.sh
./tests/scripts/run_host_tests.sh
```

### Manual CMake

```bash
cmake -S tests/host -B tests/host/build -DCMAKE_BUILD_TYPE=Debug
cmake --build tests/host/build
./tests/host/build/esp_rtl_sdr_host_tests   # or .exe on Windows
```

Expected: `RESULT passed=N failed=0` and exit code 0.

### What is under test

| Area | Examples |
|---|---|
| Version | macros match packed `get_version()` |
| Capabilities | STREAM, CONTINUOUS_RATE, NEED, HEALTH, PASSPORT on; BIAS off |
| Rates | 960k/2048k/1536k ok; gap 500k rejected; quantize |
| Recommended list | `get_supported_rates` non-empty; all in-window |
| Frequency | normalize 1 kHz quant; presets; out-of-range reject |
| Config validate | struct_size, xfer size/count, stream BAD_RATE/BAD_FREQ |
| Names | state/err strings stable |
| Passport opts | defaults |

Source under test: `src/esp_rtl_sdr_policy.cpp` (no FreeRTOS/USB).  
Driver body: `src/esp_rtl_sdr.cpp` (not host-linked).

---

## CI

Workflow: `.github/workflows/ci.yml`

1. **host-policy** — build + run host tests on Ubuntu  
2. **truth-hygiene** — header version matches `idf_component.yml` / `library.json`;
   `PROJECT_TRUTH.md` and AI disclosure / SECURITY present  

A red CI means **do not claim green policy** in docs or releases.

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
