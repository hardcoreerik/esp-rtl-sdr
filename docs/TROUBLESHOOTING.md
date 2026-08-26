# Troubleshooting — esp_rtl_sdr

Quick failures first. Deep API: [`API_REFERENCE.md`](API_REFERENCE.md).  
Truth of claims: [`../PROJECT_TRUTH.md`](../PROJECT_TRUTH.md).

---

## Build / link

| Symptom | Checks |
|---|---|
| `esp_rtl_sdr.h` not found | Component on `EXTRA_COMPONENT_DIRS` or under `components/esp_rtl_sdr`; `REQUIRES esp_rtl_sdr` |
| Wrong target | `idf.py set-target esp32p4` — P4 only claimed |
| IDF too old | Need **≥ 5.3** with esp32p4 |
| Host tests fail | `tests/scripts/run_host_tests.ps1` (or `.sh`); no IDF required |

---

## Install / start

| Error / symptom | Meaning | What to do |
|---|---|---|
| `ERR_NO_DEVICE` | No accepted Blog V4 | Plug into **USB Host** port (not UART/flash); check VBUS; `refresh_device_list` |
| `ERR_NOT_V4` / `UNSUPPORTED_DEVICE` | Stick present, identity rejected | Need official Blog V4 `0bda:2838` + strings; generic eBay RTL not claimed |
| `ERR_BUSY` on start | Already streaming / stopping | `stop` first; check `get_state` |
| `ERR_BAD_RATE` | Outside windows / quantize fail | See [`RATES.md`](RATES.md); low min **225001** Hz |
| `ERR_BAD_FREQ` | LO policy reject | 24 MHz…1766 MHz, 1 kHz quant; not 0 |
| `ERR_REENTRANT` | Lifecycle from callback | Defer start/stop/uninstall/reset to app task |
| `ERR_USB` / `TIMEOUT` | Control or bulk path | Cable, power, hub; retry; check logs |
| `ERR_FAULT` | Handle faulted | `reset` if idle, else `uninstall` |
| Start fails, state still IDLE | Fail-closed success path | Expected — no half-open claim |

---

## Streaming quality

| Symptom | Metrics / health | Action |
|---|---|---|
| Overruns climb | `metrics.overruns`, `HEALTH_USB_STARVING` | Lower SPS; raise `transfer_count`/`bytes`; free Core 0 load |
| Consumer drops | `consumer_drops`, `HEALTH_APP_TOO_SLOW` | Faster callback / larger app ring; use `read` on dedicated task |
| RF clipping | `HEALTH_RF_CLIPPING`, high sample max | Attenuate; lower `set_tuner_gain` (manual ladder 0.0…49.6 dB) |
| RF weak | `HEALTH_RF_WEAK` | Antenna; expect no gain API yet |
| Effective SPS << programmed | `efficiency` in health | Passport `probe_rates`; pick `NEED_MAX_STABLE` |

---

## Retune / callback

| Symptom | Cause | Fix |
|---|---|---|
| Retune from IQ callback seems delayed | **Async queue** (0.7.3+) | Wait for `EVT_RETUNED`; OK by design |
| Rate change while streaming | Phase 1: stop/start required | `ERR_BUSY` on `set_sample_rate` while streaming |
| Crash after uninstall | Stale handle use | Set pointer NULL; expect `STALE_HANDLE` |

---

## Multi-device

| Symptom | Fix |
|---|---|
| Count 0 with stick plugged | Wrong port; not Blog V4; call `refresh_device_list` after plug |
| `ERR_BAD_DEVICE` | Index/serial not in list; refresh and re-enumerate |

---

## Gain / bias “not working”

**0.7.5+:** CAP_GAIN / CAP_BIAS_TEE are **on** (measured Blog V4).

| Symptom | Check |
|---|---|
| `ERR_NOT_CLAIMED` | Call after successful `start` (interface must be claimed) |
| `ERR_NOT_CLAIMED` on `set_tuner_gain_mode` / `set_rtl_agc` | Call after `start` (interface claimed) |
| `ERR_REENTRANT` on gain/AGC | Do not call from the IQ event callback |
| `get_tuner_gain` / `get_*_agc` disagrees with RF | Getters are **requested state**, not register readback. Setters are async while streaming. |
| Gain/bias no RF/DC effect on P4 | P4 re-soak still open; PC capture tables may need re-verify on host |
| Multimeter shows 0 V with bias ON | SMA DC not lab-certified yet; confirm EP0 path + supply |

See [`PHASE3_CAPTURE_REPORT.md`](PHASE3_CAPTURE_REPORT.md), [`GAIN_BIAS_CAPTURE.md`](GAIN_BIAS_CAPTURE.md).

---

## Passport / soak

| Symptom | Fix |
|---|---|
| `probe_rates` `ERR_BUSY` | Stop stream first |
| Blocks forever | `entry_count × dwell_ms` — reduce dwell for smoke |
| No stable rates | Lower URB load; check power; see soak guide |

Procedure + log template: [`SOAK.md`](SOAK.md), [`lab/SOAK_LOG_TEMPLATE.md`](lab/SOAK_LOG_TEMPLATE.md).

---

## Still stuck?

1. Log `esp_rtl_sdr_get_version_string()`, `get_state()`, `get_last_error()`, caps.  
2. Note host board, IDF version, dongle serial (ok to redact mid).  
3. Open a GitHub issue with **truth:** prefix if docs oversold something.  
4. Do not paste illegal TX recipes or private captures with secrets.
