# Capability matrix — esp_rtl_sdr vs desktop librtlsdr-class drivers

Desktop reference: **librtlsdr / rtl-sdr-blog** style API used on Windows and Linux
(`rtlsdr_open`, `set_center_freq`, `read_async`, …).

Status values match `Project_truth.md`. Phase numbers match `Roadmap.md`.

| # | Desktop capability | librtlsdr (typical) | esp_rtl_sdr 0.6.0 | Phase |
|---|---|---|---|---|
| 1 | Open / close device | `rtlsdr_open` / `close` | `install` / `uninstall` | **Done** |
| 2 | Async continuous IQ | `rtlsdr_read_async` | `start` + `EVT_IQ_BLOCK` | **Done** |
| 3 | Sync IQ read | `rtlsdr_read_sync` | `esp_rtl_sdr_read` | **Done** (0.5.0) |
| 4 | Set center frequency | `rtlsdr_set_center_freq` | `set_center_freq` / `retune_hz` / `start` | **Done** |
| 5 | Get center frequency | `rtlsdr_get_center_freq` | `get_center_freq` | **Done** |
| 6 | Set sample rate | many rates | allowlist (8 rates; see `RATES.md`) | **Done** (0.6.0 API; soak partial) |
| 7 | Get sample rate | yes | `get_sample_rate` | **Done** |
| 8 | Tuner gain modes | manual / auto | fixed path | 3 |
| 9 | Set/get tuner gain | dB steps / lists | — | 3 |
| 10 | IF / secondary gains | various | — | 4 |
| 11 | Frequency correction (ppm) | `set_freq_correction` | `set/get_freq_correction` (software LO) | **Done** (0.6.0) |
| 12 | Bias-T | common Blog V4 | CAP reserved | 3 |
| 13 | Direct sampling / HF | Blog V4 / some sticks | CAP reserved | 4 |
| 14 | Bandwidth / IF filter | various | — | 4 |
| 15 | Multi-device index | `get_device_count`, open idx | `get_device_count` / `select_device` / serial | **Done** (0.6.0) |
| 16 | USB strings / serial | yes | `get_device_info` / `get_device_at` | **Done** |
| 17 | EEPROM | yes | — | Deferred |
| 18 | Cancel async | `cancel_async` | `stop` | **Done** |
| 19 | Streaming metrics (SPS, drops) | app-side | `get_metrics` first-class | **Done (stronger)** |
| 20 | Capability discovery | weak | `get_capabilities` | **Done (stronger)** |
| 21 | Fail-closed lifecycle | ad hoc | state machine + reentrancy | **Done (stronger)** |
| 22 | rtl_tcp protocol | host apps | out of driver | App |
| 23 | Any RTL2832U + any tuner | large table in librtlsdr | profile-based; V4 only now | 4 |
| 24 | Clean-room provenance | N/A (is the reference port) | **Yes** | Policy |

## IQ format

Both ecosystems default to **unsigned 8-bit interleaved I/Q** for the bulk path
we target. Apps must not assume signed IQ unless documented.

## Interpretation

- **Done / stronger:** safe to market for ESP32-P4 Blog V4 continuous IQ apps.
- **Partial / soak open:** API present; continuous P4 evidence incomplete for some rates.
- **Phase N:** planned work; not claimed in releases until Project_truth moves.
