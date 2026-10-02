# Capability matrix — esp_rtl_sdr vs desktop librtlsdr-class drivers

Desktop reference: **librtlsdr / rtl-sdr-blog**. Status matches `PROJECT_TRUTH.md`.

| # | Desktop capability | librtlsdr (typical) | esp_rtl_sdr 0.9.3 | Phase |
|---|---|---|---|---|
| 1 | Open / close | `rtlsdr_open` | `install` / `uninstall` | **Done** |
| 2 | Async IQ | `read_async` | `start` + `EVT_IQ_BLOCK` | **Done** |
| 3 | Sync IQ | `read_sync` | `read` | **Done** |
| 4–5 | Center freq | set/get | set/get + retune | **Done** |
| 6 | Sample rate | many rates | continuous in HW windows + quantize | **Done** (0.7) |
| 7 | Get sample rate | yes | exact programmed SPS | **Done** |
| 8–9 | Tuner gain | modes / steps | V4 28-step, V4L/V3c/Nooelec 29-step nominal manual ladders; board-specific Tuner AUTO and RTL AGC. V3c/Nooelec HF direct-Q bypasses tuner gain. Nooelec 2026-09-30 PC captures match every native manual pair and AUTO mode bits. | **Driver implemented; Nooelec P4 acceptance open** |
| 11 | ppm | yes | software LO offset | **Done** |
| 12 | Bias-T | common | V4/V4L/user-tested V3c GPIO ON/OFF and no-load DC measured; OFF default/stop/reattach. BlogV3 identity is generic, so UI must warn before explicit enable. **Nooelec SMArt v5 has no bias tee and rejects this API.** Loaded current unmeasured. | **Driver implemented; P4 acceptance open** |
| 12a | Tuner bandwidth | profile | At 2.4 MS/s, seven V4/V4L/V3c native-route and four V4/V4L HF-route width requests are mapped with paired PLL/demod IF. V3c boots on its proven 3.57 MHz IF and returns to that IF, with its boot filter registers, when AUTO is selected; every demod IF write is followed by a read. V3c direct-Q HF remains unsupported. Live EP0 failure rolls back or faults. Analog passbands unmeasured. | **Hardware-verified on the M5 Tab5 (V3c, V4L, V4, 96.1 MHz, 2026-09-28: see `docs/captures/v3c_live_bandwidth_2026-09-28.md`); other stations and long soak open** |
| 12b | Nooelec tuner bandwidth / IF | profile | Seven native requests at 2.4 MS/s; Nooelec filter byte `c3` with paired PLL/demod IF and settle reads. Cold tuning/return from Q uses `d3/6b`, 3.570 MHz; explicit bandwidth AUTO uses `c3/8f`, 1.815 MHz, independently captured on 2026-09-30. Q mode rejects tuner bandwidth. Older V3c AUTO policy stays V3c-specific. | **PC-measured / driver implemented / host verified; Nooelec P4 acceptance pending** |
| 13 | Direct sampling / HF | forks | V4 and V4L use separate RF+28.8e6 upconverter routes by default; V3/V3c and Nooelec use Q-branch direct sampling below the captured 24 MHz route cutoff. Nooelec accepts 100 kHz–1750 MHz; its 24 MHz PC boundary tune warned of no PLL lock (rated native floor 25 MHz). V4/V4L retain their optional direct tuner input below 28.8 MHz via `esp_rtl_sdr_set_hf_direct_min_hz()` / `esp_rtl_sdr_get_hf_direct_min_hz()`; this is distinct from Q sampling. | **Driver implemented; Nooelec P4 RF acceptance open** |
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
