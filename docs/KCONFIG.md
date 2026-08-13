# Kconfig options — esp_rtl_sdr

**Menu path (menuconfig):** `Component config` → **ESP_RTL_SDR-ESP**  
**File:** [`Kconfig`](../Kconfig) at component root.

These options are read when the component is built under ESP-IDF. Defaults feed
`esp_rtl_sdr_config_default()` for transfer size/count when Kconfig is available.

---

## Options

| Symbol | Type | Default | Range / notes |
|---|---|---|---|
| `ESP_RTL_SDR_ESP_ENABLED` | bool | `y` | Master enable for the component. |
| `ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_BYTES` | int | `16384` | Bulk URB size in bytes. Must be a **multiple of 512** for HS bulk (validated at runtime). Range 512…262144. Gate-2 multi-URB default; some Tab5 paths used 32768 historically. |
| `ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_COUNT` | int | `6` | Number of driver-owned bulk buffers. Range 2…8. Higher = more USB pipeline depth, more RAM. |
| `ESP_RTL_SDR_ESP_LOG_VERBOSE` | bool | `n` | Extra driver logs (install path, URB noise). Keep off for production serial bandwidth. |

---

## How they interact with the C API

| Layer | Wins |
|---|---|
| Kconfig defaults | Fill `config_default()` when compiled under IDF with the symbols present |
| App `esp_rtl_sdr_config_t` | **Overrides** after `config_default()` — always preferred for product tuning |
| Runtime validate | Rejects illegal sizes (not multiple of 512, out of range) |

```c
esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);   /* picks up Kconfig defaults under IDF */
cfg.transfer_bytes = 32768;         /* app override */
cfg.transfer_count = 4;
ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));
```

---

## Choosing transfer_bytes × transfer_count

| Goal | Suggestion |
|---|---|
| Continuous 960k FM-class (provenance path) | 6 × 16 KiB (defaults) |
| Higher SPS / fewer overruns | Try 6 × 32 KiB if PSRAM/internal allows |
| Tight RAM | 3–4 × 16 KiB; watch `get_metrics().overruns` |
| Passport soak | Prefer defaults for comparability |

Use `get_health()` / `get_metrics()` — not guesses — after changing URB geometry.

---

## What is *not* Kconfig (yet)

| Item | Where |
|---|---|
| USB vs delivery core pin | Compile-time constants — see [`RUNTIME_CONSTANTS.md`](RUNTIME_CONSTANTS.md) |
| Ring depth (pull IQ) | `kRingDepth` — same doc |
| Health emit period | `kHealthPeriodBlocks` — same doc |
| Rate windows / LO limits | Header macros + [`RATES.md`](RATES.md) |

Making those Kconfig is **Planned** only if product apps need it without rebuilding custom trees.

---

## menuconfig

```bash
idf.py menuconfig
# Component config → ESP_RTL_SDR-ESP
```

After changing defaults, clean rebuild if `sdkconfig` is already generated:

```bash
idf.py fullclean
idf.py build
```
