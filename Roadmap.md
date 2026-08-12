# Roadmap — esp_rtl_sdr

Close the gap with desktop librtlsdr-class drivers **without** porting them.
Evidence labels match `Project_truth.md`.

---

## North star

A stand-alone ESP-IDF component that:

1. Streams reliable CU8 IQ from RTL2832U-class dongles on ESP32-P4 HS USB.
2. Exposes a clear, discoverable API (freq, rate, gain, ppm, bias as capabilities).
3. Supports **multiple measured profiles** (Blog V4 first, then common R820T2-class).
4. Earns trust via docs, soak logs, and fail-closed behavior — not silent over-claim.

---

## Phase 0 — Stand-alone repo + rename (**this delivery**)

| Item | Status |
|---|---|
| Repo at `F:\Ai\ESP_RTL_SDR\` | **In progress** |
| Rename API to `esp_rtl_sdr` | **In progress** |
| Blog V4 profile tables preserved | **In progress** |
| `Project_truth.md` / `architecture.md` / `Roadmap.md` + docs suite | **In progress** |
| Example smoke project | **In progress** |
| Independent of OrcSDR tree edits | **Required** |

**Exit:** Local git repo; docs complete; symbols renamed; Blog V4 path source-complete.

---

## Phase 1 — API shape parity (same Blog V4 RF)

Add ergonomic APIs apps expect from desktop drivers (wrappers / small features):

- [ ] `set_center_freq` / `get_center_freq` (alias over retune / metrics)
- [ ] `set_sample_rate` / `get_sample_rate` (allowlist-enforced)
- [ ] Blocking `read` / pull of N IQ bytes from an internal ring (sync-read equivalent)
- [ ] Documented mapping table vs librtlsdr function names in `docs/CAPABILITY_MATRIX.md`
- [ ] Optional clearer device error name for non-V4 (`ERR_UNSUPPORTED_DEVICE`)

**Evidence:** Build-verified example; optional hardware re-run on Tab5 or Waveshare.

**Exit:** App authors can write open → set rate → set freq → read/async without learning OrcSDR-only names.

---

## Phase 2 — Rates, ppm, multi-device

- [ ] Measure and allowlist additional SPS only if ≥95% effective on P4 HS
- [ ] `set_freq_correction_ppm` (software LO offset first)
- [ ] Enumerate / open by index or serial when multiple dongles present
- [ ] Hardware log attached for each new rate

**Exit:** Documented rate matrix; ppm usable for known crystal offset.

---

## Phase 3 — Gain and bias-T (measured)

- [ ] Independent USB capture of gain-mode / gain-step traffic on physical Blog V4
- [ ] `set_tuner_gain_mode`, `set_tuner_gain`, `get_tuner_gains`
- [ ] Enable `CAP_GAIN` only when implemented
- [ ] Bias-T control when measured; enable `CAP_BIAS_TEE`
- [ ] Never paste librtlsdr gain tables

**Exit:** Gain and bias APIs work on Blog V4 with recorded procedure.

---

## Phase 4 — Second profile (broader RTL2832U)

- [ ] Capture init/tune for a common **R820T2** dongle (clean-room)
- [ ] `profile_r820t2` (name TBD) beside `transfers_blog_v4`
- [ ] VID/PID allowlist with fail-closed unknown
- [ ] Probe strategy documented in `docs/PROFILES.md`
- [ ] Direct sampling / HF only if separate evidence exists

**Exit:** At least two hardware-verified profiles; marketing still honest.

---

## Phase 5 — Reliability (peer “robust” bar)

- [ ] Unplug/replug recover to READY without reboot
- [ ] Five-minute soak at 960k and 2.048M with drop/SPS report checked in or linked
- [ ] Optional IQ acquire mode (`CAP_IQ_ACQUIRE`) if apps need zero-copy hold
- [ ] CI: build component + example on IDF 5.3+ (when available)

**Exit:** Reliability story competitive with vertical ESP RTL apps that used librtlsdr ports.

---

## Out of roadmap (unless evidence forces)

| Item | Why |
|---|---|
| Full SoapyRTLSDR / GQRX control surface | App territory |
| Wi-Fi rtl_tcp inside this component | Separate example/app |
| Classic ESP32 (no HS host) | Out of scope |
| Silent “try V4 tables on everything” | Violates fail-closed + clean-room honesty |

---

## Tracking

| Doc | Role |
|---|---|
| `Project_truth.md` | What is true **now** |
| `Roadmap.md` | How we get to parity |
| `docs/CAPABILITY_MATRIX.md` | Row-by-row desktop comparison |
| `CHANGELOG.md` | Released deltas |
