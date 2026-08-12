# Capability matrix — esp_rtl_sdr vs desktop librtlsdr-class drivers

Desktop reference: **librtlsdr / rtl-sdr-blog**. Status matches `PROJECT_TRUTH.md`.

| # | Desktop capability | librtlsdr (typical) | esp_rtl_sdr 0.7.0 | Phase |
|---|---|---|---|---|
| 1 | Open / close | `rtlsdr_open` | `install` / `uninstall` | **Done** |
| 2 | Async IQ | `read_async` | `start` + `EVT_IQ_BLOCK` | **Done** |
| 3 | Sync IQ | `read_sync` | `read` | **Done** |
| 4–5 | Center freq | set/get | set/get + retune | **Done** |
| 6 | Sample rate | many rates | continuous in HW windows + quantize | **Done** (0.7) |
| 7 | Get sample rate | yes | exact programmed SPS | **Done** |
| 8–9 | Tuner gain | modes / steps | — | 3 |
| 11 | ppm | yes | software LO offset | **Done** |
| 12 | Bias-T | common | CAP reserved | 3 |
| 13 | Direct sampling / HF | forks | NEED_HF LO only; V4 upconverter CAP open | 4 |
| 15 | Multi-device | index/serial | yes | **Done** |
| 19 | Metrics | app-side | `get_metrics` | **Done (stronger)** |
| 20 | Capability bits | weak | `get_capabilities` | **Done (stronger)** |
| 21 | Fail-closed lifecycle | ad hoc | state machine | **Done (stronger)** |
| 25 | Intent / mission presets | no | `apply_need` | **Done (novel)** |
| 26 | On-host rate passport | no | `probe_rates` | **Done (novel)** |
| 27 | Health narrative | no | `get_health` | **Done (novel)** |
| 23 | Any RTL + any tuner | large table | profile-based; V4 only | 4 |
| 24 | Clean-room | N/A | **Yes** | Policy |

Novel rows (25–27) are **not** librtlsdr parity — they are deliberate ESP-native
advantages documented in `docs/VISION.md`.
