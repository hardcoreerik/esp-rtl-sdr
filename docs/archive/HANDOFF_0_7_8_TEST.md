# Handoff — 0.7.8 test plan + Codex prompt

**Audience:** next Codex / Grok / human lab session.  
**Date:** 2026-08-26  
**Driver repo:** `F:\Ai\ESP_RTL_SDR` → https://github.com/hardcoreerik/esp-rtl-sdr  
**Branch / SHA:** `master` **`b74bbfe`** (PR [#7](https://github.com/hardcoreerik/esp-rtl-sdr/pull/7) merged)  
**Version in tree:** **0.7.8**  
**PROJECT_TRUTH wins** if anything here disagrees.

> **Driver EP0 is already implemented.** Do **not** re-derive Tuner AUTO / RTL AGC
> from librtlsdr. Do **not** invent IF-filter CAP. This handoff is: **test
> harness + consumer drop-in + lab evidence**.

---

## 0. Copy-paste prompt for Codex

```text
Work only in F:\Ai\ESP_RTL_SDR unless Phase B explicitly says to touch OrcSDR.

Read first, in order:
1. PROJECT_TRUTH.md
2. docs/HANDOFF_0_7_8_TEST.md          (this file — test plan is law)
3. docs/CLEAN_ROOM.md
4. docs/AGC_IF_CAPTURE.md             (measured bytes; IF was USB-silent)
5. include/esp_rtl_sdr.h              (0.7.8 CAP_GAIN_AUTO, CAP_RTL_AGC, set_rtl_agc)
6. examples/p4_serial_smoke/main/main.c
7. docs/TESTING_GUIDE.md
8. docs/SOAK.md + docs/lab/SOAK_LOG_TEMPLATE.md

Context (do not re-litigate):
- esp_rtl_sdr 0.7.8 is ON master (b74bbfe / PR #7). CI green (hygiene, host tests
  Win+Ubuntu, P4 compile IDF 5.3.2 + 5.4.1).
- Tuner AUTO: measured IR 05=E8 07=78 0C=6B. CAP_GAIN_AUTO.
- RTL digital AGC: demod 0x19 ON=0x25 OFF=0x05. CAP_RTL_AGC. Additive API.
- SDR# Bandwidth / IF filter: USB silent after open. NO CAP_IF_FILTER. Ever.
- MANUAL ladder + bias-T unchanged. Drop-in: apps that only use MANUAL/GAIN/BIAS
  must keep working. Do not rename APIs. Do not change CAP_GAIN meaning.
- Clean-room: no librtlsdr / osmocom / kvhnuke register paste.
- Do not mark Hardware-verified in PROJECT_TRUTH until a soak log from THIS tree
  exists. Implemented ≠ Hardware-verified.

Your job is NOT to re-implement AUTO EP0. Your job is:

Phase A — this repo (required)
1. Extend examples/p4_serial_smoke so that AFTER a successful start() it
   exercises 0.7.8 AGC APIs and prints a SMOKE-style PASS/FAIL matrix
   (see §4 of HANDOFF_0_7_8_TEST.md). Keep existing helper/install/need/read
   behavior. If no dongle, start→NO_DEVICE and SKIP hardware AGC rows (do not
   FAIL them). If dongle present, run T5–T14.
2. Optional but preferred: a tiny serial CLI on UART (115200) so a human can
   type GAIN / GAINMODE / RTLAGC / METRICS / STOP without rebuilding. If you
   add CLI, keep one-shot matrix as well (CI compile must still succeed).
3. Host tests already cover table constants. Add only policy tests that stay
   host-linkable (no USB). Do not try to unit-test EP0 on the PC.
4. Update examples/p4_serial_smoke/README.md with the new serial/smoke output.
5. Do not commit esp_rtl_sdr.zip. Do not vendor pcapng (hundreds of MB).

Phase B — OrcSDR consumer (only if the user confirms; different workspace)
Path: F:\Ai\OrcSDR_Waveshare
- It currently pins esp_rtl_sdr 0.7.5 via file:// F:/Ai/ESP_RTL_SDR.
- cmd_print_caps does NOT print GAIN_AUTO / RTL_AGC / HF bits.
- cmd_smoke_gain_bias() currently REQUIRES AUTO to return ERR_UNSUPPORTED
  (check "gain_mode_auto_unsupported"). After 0.7.8 that check is a FALSE FAIL.
  Invert it: AUTO must return ESP_OK; add CAP_GAIN_AUTO / CAP_RTL_AGC checks;
  add RTLAGC ON|OFF command. Do not confuse Serial "AGC ON|OFF" (FM DSP audio)
  with tuner AUTO or set_rtl_agc.
- Rebuild/flash Waveshare P4 only when the user has the board.

Phase C — lab (human + you logging)
Execute §5 script. Fill docs/lab/SOAK_P4_AGC_YYYYMMDD.md from the template.
Only then update PROJECT_TRUTH labels (Implemented stays; Hardware-verified
only with that log).

Rules:
- Fail closed. Unknown dongle still ERR_NOT_V4.
- Do not enable CAP bits that are not implemented.
- Prefer same-PR docs+code. Verbose serial logs. No oversell.
- If you change public headers, bump is already 0.7.8 — do not bump to 0.7.9
  unless you add another CAP/API.

When done: host tests + hygiene green, smoke still compiles for esp32p4,
commit on a feature branch, PR against master.
```

---

## 1. What is already true (do not redo)

| Item | State |
|---|---|
| Tuner AUTO EP0 tables | `private/measured_gain_bias_v4.hpp` (`kMeasuredV4TunerAgcReg*`) |
| RTL AGC tables | same file (`kMeasuredV4RtlAgc*`) |
| `set_tuner_gain_mode(AUTO)` | Applies after claim; async sideband queue while streaming |
| `set_rtl_agc` / `get_rtl_agc` | Additive |
| `CAP_GAIN_AUTO` bit 18, `CAP_RTL_AGC` bit 19 | On in `esp_rtl_sdr_get_capabilities()` |
| IF filter CAP | **Off.** Capture USB-silent. |
| CI | Green on PR #7 |
| P4 USB re-soak of AUTO | **Still open** |
| `examples/p4_serial_smoke` | Compiles; **does not** call AUTO / `set_rtl_agc` yet |
| OrcSDR Waveshare | Still docs **0.7.5**; SMOKE **expects AUTO = UNSUPPORTED** (will false-fail on 0.7.8) |

Measured AUTO trio (do not “improve” from memory):

```text
reg 0x05 = 0xE8
reg 0x07 = 0x78
reg 0x0C = 0x6B     # manual ladder 0x0C is always 0x68
```

RTL AGC:

```text
wValue 0x1920  wIndex 0x0010  data ON=0x25  OFF=0x05
```

---

## 2. Drop-in contract (break these = bug)

Apps written against 0.7.5–0.7.7 that **never call AUTO**:

1. `CAP_GAIN` still means **manual ladder**. Do not overload it.
2. `set_tuner_gain` still forces MANUAL and still needs a claimed stream.
3. Bias-T, retune, rates, HF upconverter, delivery modes: unchanged.
4. New bits are **additive**. Old `CAPS` printers that only mask known bits stay valid.
5. `set_tuner_gain_mode(AUTO)` **behavior change:** was `ERR_UNSUPPORTED`, now `ESP_OK` after `start`. That is intentional 0.7.8. Consumers that asserted UNSUPPORTED **must** be updated (OrcSDR SMOKE).
6. Unclaimed AUTO → `ERR_NOT_CLAIMED` (not UNSUPPORTED).
7. From IQ callback → `ERR_REENTRANT` (same as `set_tuner_gain`).
8. Default `get_tuner_gain_mode()` may still read AUTO before any EP0. First explicit AUTO after `start` **must** write hardware (`tuner_auto_applied` flag). Treating “default AUTO” as already applied is a bug.
9. `ERR_REENTRANT` is **callback-task only**. An app task must not get REENTRANT solely because `EVT_IQ_BLOCK` is running on the delivery task (Tab5 L4 run 1).

---

## 3. Test layers (what proves what)

| ID | Layer | Environment | Proves | Does not prove |
|---|---|---|---|---|
| L0 | Hygiene | `tests/scripts/check_truth_hygiene.sh` | Version 0.7.8 everywhere; MEASURED_2026_08_26 marker | USB |
| L1 | Host policy | `tests/host` | AUTO trio ≠ any manual step; RTL constants; CAP bits | EP0 |
| L2 | IDF compile | `examples/p4_serial_smoke` esp32p4 | New symbols link | Runtime |
| L3 | Fail-closed | P4, **no** dongle | `NO_DEVICE`; no FAULT hang | AGC |
| L4 | Stream + AGC matrix | P4 + Blog V4 | EP0 + USB still alive | Hours-long soak |
| L5 | Consumer | OrcSDR Waveshare | Drop-in + SMOKE | This repo isolation |
| L6 | Soak | ≥ 15 min then ≥ 1 h | Overruns bounded | Production warranty |

L0–L2 are CI. L3–L6 are lab. **Bugs in 0.7.8 AGC live in L4.**

---

## 4. Phase A — smoke matrix (implement this)

After `esp_rtl_sdr_start` returns `ESP_OK`, run the following and log each line
`SMOKE <name> PASS|FAIL`. If start is `NO_DEVICE`, print `SMOKE skip hardware (NO_DEVICE)` and still PASS the overall smoke (helpers already ran).

Use `esp_rtl_sdr_err_to_name` on every `esp_err_t`.

### 4.1 Capability discovery

| Name | Assert |
|---|---|
| `CAP_GAIN` | bit 15 on |
| `CAP_GAIN_AUTO` | bit 18 on |
| `CAP_RTL_AGC` | bit 19 on |
| `CAP_BIAS_TEE` | on |
| `version` | `esp_rtl_sdr_get_version_string()` is `0.7.8` |

### 4.2 Fail-closed (may run even after start if you also test a second handle — optional)

On the **live** handle:

| Name | Call | Expect |
|---|---|---|
| `auto_from_callback` | If easy: skip; else document “not automated” | `ERR_REENTRANT` if called from `event_cb` |

Before start is already over in current smoke. Optional extra: a dedicated
unclaimed probe is **not** required if we document that `NOT_CLAIMED` is covered
by calling AUTO only after start in this matrix, and OrcSDR GAINMODE before START.

### 4.3 MANUAL regression (must not break 0.7.5)

| Name | Call | Expect |
|---|---|---|
| `gain_mode_manual` | `set_tuner_gain_mode(MANUAL)` | `ESP_OK` |
| `set_gain_0` | `set_tuner_gain(0)` | `ESP_OK`; `get_tuner_gain` nearest 0 |
| `set_gain_297` | `set_tuner_gain(297)` | `ESP_OK`; applied 297 (29.7 dB — capture parked value) |
| `set_gain_400` | `set_tuner_gain(400)` | `ESP_OK`; applied 402 or 400 per ladder (40.2 dB is 402) |
| `get_mode_manual` | `get_tuner_gain_mode` | `MANUAL` |

Log `get_metrics` overruns **before** this block and **after**. Overrun delta of a few is noise; a large jump or FAULT is FAIL.

### 4.4 Tuner AUTO (the new path)

Wait ≥ 200 ms between mode changes so the delivery task can drain bulk + EP0.

| Name | Call | Expect |
|---|---|---|
| `gain_mode_auto` | `set_tuner_gain_mode(AUTO)` | `ESP_OK` (**not** UNSUPPORTED) |
| `get_mode_auto` | `get_tuner_gain_mode` | `AUTO` |
| `auto_idempotent` | AUTO again immediately | `ESP_OK` (no-op after first apply) |
| `stream_alive_after_auto` | `read` 4 KiB, timeout 1000 ms | `ESP_OK` and `n > 0` |
| `gain_forces_manual` | `set_tuner_gain(297)` then `get_tuner_gain_mode` | `ESP_OK` and mode `MANUAL` |
| `auto_again` | AUTO after that | `ESP_OK` |
| `manual_restore` | `set_tuner_gain_mode(MANUAL)` | `ESP_OK`; stream still `read`s |

### 4.5 RTL digital AGC (additive)

| Name | Call | Expect |
|---|---|---|
| `rtl_agc_on` | `set_rtl_agc(true)` | `ESP_OK` |
| `rtl_agc_get_on` | `get_rtl_agc` | true |
| `rtl_agc_off` | `set_rtl_agc(false)` | `ESP_OK` |
| `rtl_agc_get_off` | `get_rtl_agc` | false |
| `rtl_agc_independent` | AUTO tuner, then `set_rtl_agc(true)` | both `ESP_OK`; tuner mode still AUTO |
| `stream_alive_after_rtl_agc` | `read` 4 KiB | `n > 0` |

### 4.6 USB health (bug hunt)

After the matrix, log:

```
metrics.bytes_received
metrics.overruns
metrics.consumer_drops
metrics.effective_sps
health.overall
health.advice
```

**FAIL** if: `FAULT`, `ERR_USB`, overruns grow unbounded during the ~2 s matrix, or `read` returns 0 after AUTO.

**Do not FAIL** on RF clip/weak — antenna is optional for USB-path bugs.

### 4.8 Honesty bound (do not overclaim)

Live **setters** (gain, tuner AUTO, RTL AGC, bias) are **asynchronous** while
streaming. **Getters** return the driver’s requested-state shadow, not a
hardware-register readback.

The smoke **may** claim:

- `ESP_OK` / `NOT_CLAIMED` / `REENTRANT` contract
- CAP bits and version string
- IQ `read` still returns bytes after a transition
- metrics / health did not explode

The smoke **must not** claim:

- “AUTO trio was verified on the R828D”
- “demod 0x19 read back as 0x25”
- Hardware-verified EP0 from getters matching USBPcap

EP0 contents were proven on the **PC capture** (`docs/AGC_IF_CAPTURE.md`). P4
path: infer from API + stream survival until a soak log or a future readback CAP.

### 4.7 Suggested serial CLI (if implemented)

Keep commands short, uppercase, one line:

```
HELP
CAPS
START / STOP
FREQ <hz>
GAIN <tenth>
GAIN?
GAINMODE MANUAL|AUTO
RTLAGC ON|OFF | RTLAGC?
METRICS
HEALTH
SMOKE
```

115200 8N1. Do not block the USB owner task in the CLI.

---

## 5. Lab script (human + Codex logging)

Gear: Waveshare P4 (or Tab5), Blog V4 on **USB Host** (Waveshare: lower-left Type-A by Ethernet, OTG jumper HOST). Console on Type-C CH343. Antenna on 96.1 MHz if you want RF, not required for USB bugs.

### 5.1 Build this tree

```text
cd F:\Ai\ESP_RTL_SDR\examples\p4_serial_smoke
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash monitor
```

Expect boot log `esp_rtl_sdr 0.7.8`. If it says 0.7.7, wrong tree.

### 5.2 No-dongle pass

Unplug Blog V4. `start -> ESP_RTL_SDR_ERR_NO_DEVICE`. Process must reach `smoke complete` / CLI prompt. Hang = lifecycle bug.

### 5.3 Dongle pass

Plug Blog V4. Reboot or wait for hotplug if implemented.

Run SMOKE matrix (one-shot or `SMOKE` command).

Then interactive (if CLI exists):

```
GAINMODE AUTO
METRICS          # wait 3s, METRICS again — overruns should not explode
GAINMODE MANUAL
GAIN 297
RTLAGC ON
RTLAGC OFF
FREQ 1090000000  # retune while streaming if API used via retune_hz / set_center_freq
METRICS
STOP
START
GAINMODE AUTO
```

Listen (optional): 96.1 WFM in OrcSDR. AUTO should not permanently mute; MANUAL + GAIN 297 should sound like the capture session.

### 5.4 HF + AUTO (optional, honest)

`FREQ 10000000` (WWV) if `CAP_HF_UPCONVERTER`. Then `GAINMODE AUTO`.

Known risk: we **skip** triplexer filter rewrite after AUTO so `0x0C=0x6B` is not smashed. HF may sound odd on AUTO. **Document**, do not “fix” by writing 0x68 after AUTO. MANUAL after AUTO should restore ladder + band FE.

### 5.5 OrcSDR (Phase B)

Today:

```
CAPS ... gain=1 ...          # does not print GAIN_AUTO
SMOKE gain_mode_auto_unsupported PASS   # on 0.7.7
```

On 0.7.8 **without** OrcSDR patch, that SMOKE row **FAILS** even if the driver is correct.

After patch:

```
MODE FM
START
CAPS                         # gain_auto=1 rtl_agc=1
SMOKE                        # AUTO must PASS as ESP_OK
GAINMODE AUTO
GAINMODE MANUAL
```

Serial `AGC ON|OFF` is **FM audio DSP**. Never wire that to `set_tuner_gain_mode` or `set_rtl_agc`.

---

## 6. Bug catalog (what to file vs ignore)

| Symptom | Verdict |
|---|---|
| AUTO → `ERR_UNSUPPORTED` | App/binary is 0.7.7. Not a 0.7.8 driver bug. |
| AUTO before START → `NOT_CLAIMED` | Correct. |
| AUTO from event_cb → `REENTRANT` | Correct. |
| Stream dies when AUTO | **Bug** — sideband pause/queue. |
| Overruns +10 while toggling, then flat | Likely OK (one drain window). |
| Overruns climb every toggle with no bound | **Bug**. |
| AUTO then `0x0C` band FE restored to 0x68 | **Bug** if someone reintroduced `run_band_frontend_after_gain` after AUTO. |
| SMOKE in OrcSDR fails `gain_mode_auto_unsupported` | **Consumer bug** (stale assert). Fix OrcSDR. |
| SDR# Bandwidth still not in driver | Not a bug. No EP0. |
| Audio AGC in OrcSDR unchanged | Not this driver. |

---

## 7. Pass / fail for the Codex PR

**Must**

- [ ] Hygiene script OK at 0.7.8  
- [ ] Host tests still pass (including `test_measured_agc_tables`)  
- [ ] `p4_serial_smoke` still `idf.py build` for esp32p4 (CI)  
- [ ] Smoke prints CAP_GAIN_AUTO / CAP_RTL_AGC  
- [ ] With dongle, `gain_mode_auto` is PASS (`ESP_OK`)  
- [ ] MANUAL set_gain still PASS  
- [ ] No IF CAP invented  
- [ ] PR description cites this handoff  

**Should**

- [ ] Serial CLI for human toggles  
- [ ] Metrics logged before/after AUTO  
- [ ] OrcSDR SMOKE inverted + CAPS bits (Phase B, separate PR if needed)  

**Must not**

- [ ] Paste librtlsdr  
- [ ] Change MANUAL ladder bytes  
- [ ] Mark Hardware-verified without `docs/lab/SOAK_*.md`  
- [ ] Commit `esp_rtl_sdr.zip` or 100 MB pcaps  

---

## 8. Soak log (only path to Hardware-verified)

Copy `docs/lab/SOAK_LOG_TEMPLATE.md` → `docs/lab/SOAK_P4_AGC_20260826.md` (use real date).

Add rows:

| Extra | Value |
|---|---|
| AUTO toggles | count / errors |
| RTL AGC toggles | count / errors |
| overrun delta during toggles | |
| `read` after AUTO | bytes |

Duration: 15 min is a **lab note**. ≥ 1 h continuous at 960k or 2.048M with periodic AUTO/MANUAL is the soak bar (`docs/SOAK.md`).

---

## 9. Files Codex is expected to touch (Phase A)

| Path | Why |
|---|---|
| `examples/p4_serial_smoke/main/main.c` | AGC matrix ± CLI |
| `examples/p4_serial_smoke/README.md` | How to read PASS/FAIL |
| `examples/p4_serial_smoke/main/CMakeLists.txt` | Only if new sources |
| `docs/TESTING_GUIDE.md` | Point at this handoff / new smoke rows |
| `docs/lab/SOAK_*.md` | Only after a real run |

Do **not** retouch `private/measured_gain_bias_v4.hpp` unless a lab recapture contradicts it (then stop and update PROJECT_TRUTH — that is a new capture, not this task).

---

## 10. Working directory cheat sheet

```text
Driver:     F:\Ai\ESP_RTL_SDR
Smoke:      F:\Ai\ESP_RTL_SDR\examples\p4_serial_smoke
OrcSDR:     F:\Ai\OrcSDR_Waveshare     (Phase B only)
Lab pcaps:  C:\Tools\esp-rtl-sdr-lab\captures   (evidence; do not git-add)
```

Remote: https://github.com/hardcoreerik/esp-rtl-sdr  
Default branch: **master** (there is no `main`).
