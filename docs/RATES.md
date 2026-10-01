# Sample rates — esp_rtl_sdr

**Version:** 0.7.0  
**Policy:** any rate **within hardware windows** after quantize. Recommended
named rates remain for discovery and passport defaults.

Sample-rate policy is independent of center-frequency policy. `CUSTOM_HZ`
frequency requests preserve exact Hz; no sample-rate window changed.

Programming: RTL2832U resampler

```text
ratio_reg = ((28.8e6 << 22) / sps) & 0x0ffffffc
realized  = ratio_reg | ((ratio_reg & 0x08000000) << 1)
exact_sps = (28.8e6 << 22) / realized
```

`ratio_reg` is what demod bytes `0x9f..0xa2` store (bit 28 clear). The demod
mirrors bit 27 into bit 28, so Hz has to be computed from `realized`. Skipping
that mirror reports every low-band rate at about 2× (250000 Hz becomes
562500 Hz). 900000 Hz stores `0x08000000` and realizes as 300000 Hz, so it is
rejected.

---

## Hardware windows

| Window | Hz | Notes |
|---|---|---|
| Low | **225001** – 300000 | Historical RTL low band; **225000 rejected** (ratio mask zeroes) |
| High | **900001** – 3200000 | Primary SDR band. **900000 rejected** (bit 27 mirrors to 300 kHz) |
| Gap | 300001 – 900000 | **Rejected** |
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
