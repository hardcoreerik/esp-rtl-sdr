# Sample rates — esp_rtl_sdr

**Version:** 0.6.0  
**Policy:** only allowlisted rates may be programmed. Apps use
`esp_rtl_sdr_is_rate_supported()` / `esp_rtl_sdr_get_supported_rates()`.

Programming uses the RTL2832U resampler ratio formula  
`ratio = (28.8e6 << 22) / sample_rate_sps` (masked), via clean-room Blog V4
control records — not a librtlsdr table paste.

---

## Allowlist (0.6.0)

| Macro | SPS | Evidence | Notes |
|---|---:|---|---|
| `ESP_RTL_SDR_RATE_250K` | 250000 | **Formula** | Desktop-common; P4 continuous soak **not** claimed |
| `ESP_RTL_SDR_RATE_256K` | 256000 | **Formula** | Desktop-common; P4 soak open |
| `ESP_RTL_SDR_RATE_960K` | 960000 | **Provenance (P4)** | OrcSDR Tab5 / Waveshare continuous FM-class path |
| `ESP_RTL_SDR_RATE_1024K` | 1024000 | **Formula + prior allowlist** | Programmed; dedicated P4 soak log open |
| `ESP_RTL_SDR_RATE_1800K` | 1800000 | **Formula** | P4 soak open |
| `ESP_RTL_SDR_RATE_2048K` | 2048000 | **Provenance (P4)** | OrcSDR ADS-B path on Tab5 / Waveshare |
| `ESP_RTL_SDR_RATE_2400K` | 2400000 | **Formula** | PC clean-room capture rate used elsewhere; P4 soak open |
| `ESP_RTL_SDR_RATE_3200K` | 3200000 | **Formula** | Near upper practical HS bulk; P4 soak open |

**Evidence labels** match `Project_truth.md`. **Provenance** means measured under
another project with the same Blog V4 transfer tables; re-soak from *this* tree
is still open.

---

## What “supported” means

| Claim | Meaning |
|---|---|
| On the allowlist | Driver will accept the rate and program the RTL ratio |
| P4 continuous path | Observed sustainable bulk IQ on ESP32-P4 HS with Blog V4 |
| Formula-only | Ratio math + EP0 sequence exist; long-run drop rate not certified here |

Apps that need a guaranteed effective SPS should prefer **960k** or **2048k**
until a formal soak artifact is checked in (Roadmap Phase 5).

---

## Mid-stream rate change

`set_sample_rate` while **STREAMING** returns `ESP_RTL_SDR_ERR_BUSY`.  
Stop, set rate (or pass rate in `start` / `start_hz`), then start again.

---

## Adding a rate (policy)

1. Confirm the rate is useful and within RTL2832 practical range.  
2. Add macro + allowlist entry.  
3. Document evidence in this file (**Formula** until P4 log exists).  
4. Prefer a short soak (≥95% effective SPS target from Roadmap Phase 2) before
   marketing as “P4 continuous.”  
5. Never copy librtlsdr rate tables into source as authority.

---

## Related

- Header: `ESP_RTL_SDR_RATE_*` in `include/esp_rtl_sdr.h`
- Implementation: `kAllowRates[]`, `run_sample_rate()` in `src/esp_rtl_sdr.cpp`
- Roadmap Phase 2 / Phase 5 soak tracking
