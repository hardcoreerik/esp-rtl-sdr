# Sync status — exp/multi-dongle-0.8.0-rc1

## V4 bring-up audit vs `codex/v0.7.15-v4-hf-routing` (1cd19d1)

**No accidental V4 bring-up rewrites. Nothing reverted.**

| Path / area | Verdict |
|---|---|
| `private/measured_gain_bias_v4.hpp` | **Unchanged** (byte-identical) |
| `kRtlInitTransfers` / cleanup / final-tune tables | **Byte-identical** (struct only moved to `rtl_control.hpp`) |
| `measured_v4_frontend_plan` | **Unchanged** |
| `run_band_frontend` | **Gate-only** — early-return unless `rtl_profile_uses_v4_hf_routing`; BlogV4 still calls `measured_v4_frontend_plan` identically |
| `run_tune` HF offset | **Gate-only** — `rtl_profile_tuner_frequency_hz(BlogV4)` == prior `esp_rtl_sdr_tuner_frequency_hz` |
| `map_tuner_record_for_profile` | **Gate-only** — remaps `0x74→0x34` only when profile I2C ≠ V4; BlogV4 identity map |
| Gain / Bias / AUTO CAP checks | **Gate-only** — BlogV4 device caps still include full library set |
| `run_cleanup_best_effort` | **Gate-only** — BlogV4-only (same cleanup table) |

## Host tests (box)
- policy: 373 passed, 0 failed
- profiles: 76 passed, 0 failed (includes PR #18 HF/VHF/UHF frontend regression)
- TRUTH_HYGIENE_OK ver=0.8.0-rc1
