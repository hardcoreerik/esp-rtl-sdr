# Sample rates — esp_rtl_sdr

**Version:** 0.7.0  
**Policy:** any rate **within hardware windows** after quantize. Recommended
named rates remain for discovery and passport defaults.

Programming: RTL2832U resampler  
`ratio = (28.8e6 << 22) / sps` (masked `& ~3`), via clean-room Blog V4 EP0 slice.

---

## Hardware windows

| Window | Hz | Notes |
|---|---|---|
| Low | 225000 – 300000 | Historical RTL low band |
| High | 900000 – 3200000 | Primary SDR band |
| Gap | 300001 – 899999 | **Rejected** (unstable ecosystem-wide) |
| Vendor stable claim | ≤ 2560000 | Blog V4 datasheet “stable” |
| Vendor max | 3200000 | “with drops” per Blog V4 DS |

API:

- `esp_rtl_sdr_is_rate_supported(sps)` — in window and quantizable  
- `esp_rtl_sdr_quantize_sample_rate(req, &exact)` — exact programmed SPS  
- `set_sample_rate` / `start` store and program **exact**  
- `get_supported_rates` — **recommended** list only (not every integer)

---

## Recommended list (discovery / passport)

| Macro | SPS | Evidence |
|---|---:|---|
| `RATE_250K` | 250000 | Formula |
| `RATE_256K` | 256000 | Formula |
| `RATE_960K` | 960000 | **Provenance (P4)** |
| `RATE_1024K` | 1024000 | Formula + prior allowlist |
| `RATE_1800K` | 1800000 | Formula |
| `RATE_2048K` | 2048000 | **Provenance (P4)** |
| `RATE_2400K` | 2400000 | Formula |
| `RATE_2560K` | 2560000 | Vendor stable ceiling |
| `RATE_3200K` | 3200000 | Formula; drops expected |

---

## Passport

`esp_rtl_sdr_probe_rates()` streams each recommended (or extended) rate for
`dwell_ms`, measures `effective_sps` / drops, marks **stable** if efficiency
≥ `min_efficiency_pct` (default 95). Result:

- `best_stable_sps` for `NEED_MAX_STABLE`  
- full `entries[]` for apps / logs  

This is **learned on this host**, not a universal claim.

---

## Mid-stream rate change

Still **BUSY** while streaming — stop / set / start.

---

## Related

- `docs/VISION.md` — passport in the nervous-system model  
- `docs/SILICON.md` — resampler + DS  
- `docs/TESTING.md` — lab gear for soak  
