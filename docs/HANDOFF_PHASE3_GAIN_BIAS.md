# Handoff — Phase 3 gain / bias CAP (issue #1)

**Audience:** Next agent session + human maintainer (Erik).  
**Date:** 2026-08-13 (post-reboot of capture PC)  
**Repo:** `F:\Ai\ESP_RTL_SDR` → https://github.com/hardcoreerik/esp-rtl-sdr  
**Branch:** `master` (tip at handoff: `31b574c` — confirm with `git log -1`)  
**Version in tree:** **0.7.4** (tag `v0.7.4` released)  
**Tracking issue:** https://github.com/hardcoreerik/esp-rtl-sdr/issues/1  

> **Superseded for implementation:** tables + CAP shipped in **v0.7.5**. Remaining:
> P4 re-soak, optional multimeter DC, AUTO AGC. Keep this file as capture/runbook history.

**Primary goal of next work (historical):** Complete **Phase 3** so `CAP_GAIN` and `CAP_BIAS_TEE` can be enabled honestly — measured USB evidence first, then clean-room implementation, never librtlsdr paste.

---

## 1. Session start (copy this)

### Working directory

```text
F:\Ai\ESP_RTL_SDR
```

### Prompt for a new Grok / Claude / Codex session

```text
Work only in F:\Ai\ESP_RTL_SDR (esp_rtl_sdr, github.com/hardcoreerik/esp-rtl-sdr).

Read first (in order):
1. docs/HANDOFF_PHASE3_GAIN_BIAS.md  (this handoff)
2. PROJECT_TRUTH.md
3. docs/GAIN_BIAS_CAPTURE.md
4. docs/PHASE3_PC_SETUP.md
5. docs/CLEAN_ROOM.md
6. Issue #1: phase-3 gain and bias-T USB capture

Context: We are finishing Phase 3 for Blog V4 gain + bias-T. CAP_GAIN and
CAP_BIAS_TEE are still OFF; set_* APIs return ERR_UNSUPPORTED. Desktop tools
on this Windows PC are black-box stimulus only. Clean-room: implement only from
our USB captures. Do not paste librtlsdr / rtl-sdr-blog source.

PC lab tools (already installed): C:\Tools\esp-rtl-sdr-lab\  (see README there).
Blog V4 plugs into THIS PC for capture (not ESP32-P4). TinySA is optional RF
relative check via SMA IN/OUT; USB to TinySA is serial control only.

Immediate plan:
A) Post-reboot verify: tshark -D shows USBPcap*, rtl_test sees Blog V4
B) Bias capture: Wireshark USBPcap before open → rtl_biast on/off → pcapng + NOTES
C) Gain capture: same + SDR#/SDR++ manual gain steps → pcapng + NOTES
D) Analyze EP0; extract sequences into private/profile tables
E) Wire set_tuner_gain* / set_bias_tee; enable CAP bits only when hardware works
F) Host tests + CAP hygiene still green; update PROJECT_TRUTH; close #1 when done

Honesty rules: PROJECT_TRUTH wins; no CAP on without capture; no oversell.
Prefer same-PR docs+code. Do not edit OrcSDR monorepo for this work.
```

### Shell bootstrap (after reboot)

```powershell
cd F:\Ai\ESP_RTL_SDR
$env:Path = "C:\Tools\esp-rtl-sdr-lab\bin;C:\Program Files\Wireshark;" + $env:Path

# Verify capture stack
tshark -D
# Expect USBPcap1, USBPcap2, ... plus NPF network adapters

# Plug Blog V4 into PC USB, then:
rtl_test -t

# Optional full check:
C:\Tools\esp-rtl-sdr-lab\verify_lab.ps1
```

---

## 2. What this project is (one paragraph)

**esp_rtl_sdr** is a **stand-alone ESP-IDF USB Host driver** for RTL2832U-class dongles, **Blog V4 first** (`0bda:2838`), on **ESP32-P4 HS USB**. Not a librtlsdr port. Fail-closed C API, continuous rates, health/passport/need, async retune, delivery modes (0.7.4). Board UI stays in apps (e.g. OrcSDR). **PROJECT_TRUTH.md** is authoritative for claims.

---

## 3. Work completed in recent sessions (verbose)

### 3.1 Product / driver software (0.7.x)

| Version | What landed |
|---|---|
| **0.7.0–0.7.1** | Continuous rates, need/health/passport, host policy tests, CI hygiene |
| **0.7.2** | Runtime hardening (STARTING, task join, transactional ring, Kconfig, etc.) |
| **0.7.3** | True **async retune** from event callback + `EVT_RETUNED` |
| **0.7.4** | **Delivery modes** BOTH / CALLBACK / READ + **lazy pull ring**; `CAP_DELIVERY_MODE`; CodeRabbit fix to serialize `ensure_pull_ring` |

**PR #6** merged: https://github.com/hardcoreerik/esp-rtl-sdr/pull/6  
**Release:** https://github.com/hardcoreerik/esp-rtl-sdr/releases/tag/v0.7.4  

Gain/bias APIs exist as **stubs** only (`ERR_UNSUPPORTED`); CAP bits **off**; CI guards against enabling CAP_GAIN/BIAS early (`tests/scripts/check_truth_hygiene.sh`).

### 3.2 Documentation (landing + API + process)

| Doc | Role |
|---|---|
| `README.md` | Why / MCU comparison / how-to / API map |
| `docs/API_REFERENCE.md` | Full params, returns, examples (**0.7.4**) |
| `docs/API.md` | Design contract only |
| `docs/EXAMPLES.md`, `TROUBLESHOOTING.md`, `KCONFIG.md` | Usage |
| `docs/SCOPE.md` | Intentional P4 + Blog V4 only |
| `docs/SOAK.md` + `docs/lab/SOAK_LOG_TEMPLATE.md` | Hardware soak (issue #2 — still open) |
| `docs/RUNTIME_CONSTANTS.md` | Ring / cores / health period |
| `docs/REVIEW_GAPS_2026-08.md` | External review → actions |
| `docs/GAIN_BIAS_CAPTURE.md` | Phase 3 capture procedure |
| `docs/PHASE3_PC_SETUP.md` | This PC tool install / PATH |
| `docs/LAB_HOBBYIST.md` | TinySA / desk lab honesty |
| `docs/CLEAN_ROOM.md` | No source paste rules |

### 3.3 External review gap closure

Review correctly flagged: incomplete gain/bias, narrow HW, empty issues, soak evidence, AGPL, docs, releases.

**Closed in-repo (docs/process):** API reference, Kconfig docs, soak **procedure**, SCOPE, licensing process, GitHub Release v0.7.3/v0.7.4, tracking issues.

**Issue #3 closed** (API_REFERENCE sync process — CONTRIBUTING + hygiene CI).

**Still open:**

| Issue | Topic |
|---|---|
| **#1** | Phase 3 gain/bias CAP ← **this handoff** |
| **#2** | Stand-alone P4 soak log from this tree |
| **#4** | S2/S3 FS host — Deferred (intentional) |
| **#5** | Community validation living checklist |

### 3.4 Capture PC software install (this Windows machine)

| Item | Location / notes |
|---|---|
| Wireshark | `C:\Program Files\Wireshark` |
| USBPcap | `C:\Program Files\USBPcap` — **needed reboot** after install |
| Npcap | Installed by user; `wpcap.dll` present; network capture works |
| Lab tools tree | **`C:\Tools\esp-rtl-sdr-lab\`** |
| `rtl_biast`, `rtl_test`, Blog x64 bins | `C:\Tools\esp-rtl-sdr-lab\bin` |
| Zadig, SDR#, SDR++ | Linked/copied into that `bin` |
| Captures staging | `C:\Tools\esp-rtl-sdr-lab\captures\` |
| PATH | User PATH + PowerShell profile + `cmd\labenv.cmd` |
| Verify script | `C:\Tools\esp-rtl-sdr-lab\verify_lab.ps1` |

**Pre-reboot state:** USBPcap driver installed but often **Stopped** until reboot; USBPcapCMD said soft USB restart failed → reboot required. User rebooted; next session must **verify USBPcap\* in `tshark -D`**.

### 3.5 TinySA Ultra (optional RF)

- USB to PC = **CDC serial control** + power/firmware (not RF over USB).  
- RF = SMA **IN** (spectrum) / **OUT** (generator).  
- For #1: optional relative RF / weak tone; **does not replace** Blog V4 USBPcap.  
- Keep DC (bias-T) off TinySA RF ports (DC block / multimeter for bias DC).

### 3.6 What was **not** done

- No `.pcapng` evidence package in repo  
- No measured gain/bias EP0 tables in profile  
- CAP_GAIN / CAP_BIAS_TEE still **off**  
- No P4 re-verify of gain/bias from this tree  
- Issue #1 still open  

---

## 4. Current truth (do not oversell)

From `PROJECT_TRUTH.md` discipline:

| Claim | State |
|---|---|
| Stream / retune / metrics / rates / health / passport | Implemented (Blog V4 path; soak from this tree still open) |
| Gain / bias hardware | **Planned** — stubs only |
| Production-ready | **No** (0.x) |
| librtlsdr port | **No** |

---

## 5. Plan to complete issue #1 (Phase 3)

Execute in order. Do **not** enable CAP bits before step E succeeds on hardware.

### Phase A — Post-reboot lab ready (30–60 min)

| Step | Action | Pass criteria |
|---|---|---|
| A1 | New shell, PATH includes lab bin + Wireshark | `where rtl_biast`, `where tshark` |
| A2 | `tshark -D` | Lists **USBPcap1** (etc.) |
| A3 | Plug **Blog V4** into **PC** USB | Device present |
| A4 | `rtl_test -t` | Opens device; identity consistent with V4 |
| A5 | If open fails | Zadig → WinUSB on Bulk-In / Interface 0; retry |
| A6 | Multimeter ready (optional but recommended for bias DC) | — |

### Phase B — Bias-T USB capture (core of #1)

| Step | Action | Pass criteria |
|---|---|---|
| B1 | Close all apps using dongle | — |
| B2 | Wireshark → capture on **USBPcap** for that hub/port | Capture running **before** open |
| B3 | `rtl_biast -d 0 -b 1` → wait ≥2 s → `rtl_biast -d 0 -b 0` | Commands succeed |
| B4 | Stop capture; save | `C:\Tools\esp-rtl-sdr-lab\captures\bias_on_off.pcapng` |
| B5 | Multimeter SMA center vs shield with bias ON | ~4.5 V class if DS matches; note in NOTES |
| B6 | Record serial (redact mid if public), tool versions, SHA-256 | `NOTES.md` |

Filter later in Wireshark: USB control transfers (device 0 / EP0) around toggle timestamps.

### Phase C — Gain USB capture

| Step | Action | Pass criteria |
|---|---|---|
| C1 | Wireshark USBPcap start first | — |
| C2 | SDR# or SDR++ open RTL-SDR; fixed LO (e.g. 100.1 MHz), fixed rate | Streaming |
| C3 | AGC **off**; step manual gain across several UI steps; log UI dB + wall clock | Distinct steps |
| C4 | Optional: AGC on/off once | Separate note |
| C5 | Save | `gain_steps.pcapng` + NOTES rows |
| C6 | Optional TinySA | Gen low tone or spectrum Δ only — **relative** |

### Phase D — Analyze → clean-room tables

| Step | Action | Pass criteria |
|---|---|---|
| D1 | Diff EP0 before/after bias ON vs OFF | Candidate GPIO/SYS control URBs |
| D2 | Diff EP0 across gain steps | Candidate I2C/register write sequences to R828D path |
| D3 | Cross-check names only vs public R820T2 material after capture shows which regs change | No invented regs from memory |
| D4 | Write measured sequences into profile (`private/` / transfers tables) | **Measured only**; no librtlsdr paste |
| D5 | Document authority in CLEAN_ROOM / capture NOTES | Hashes, serial, software versions |

### Phase E — Implement in esp_rtl_sdr

| Step | Action | Pass criteria |
|---|---|---|
| E1 | `set_bias_tee(true/false)` applies EP0 from capture | `ESP_OK`; DC matches when measured |
| E2 | `set_tuner_gain_mode` / `set_tuner_gain` / `get_tuner_gains` | Manual list from capture; hardware responds |
| E3 | Enable **`CAP_BIAS_TEE`** / **`CAP_GAIN`** only for working paths | Hygiene script updated if needed |
| E4 | Fail-closed: unknown / failed EP0 → error, CAP stays honest | No half-on CAP |
| E5 | Host tests still pass; CI green | Ubuntu/Windows host + hygiene + P4 compile |

### Phase F — Evidence + truth + issue close

| Step | Action | Pass criteria |
|---|---|---|
| F1 | Evidence package in `docs/captures/` **or** private vault + SHA-256 in NOTES | Reviewable |
| F2 | Update `PROJECT_TRUTH.md` (Implemented / Hardware-verified as earned) | Labels accurate |
| F3 | CHANGELOG, API_REFERENCE gain/bias sections, README stubs table | Same release |
| F4 | Optional: short P4 smoke that calls set_bias/set_gain after stream start | Provenance → re-verify path |
| F5 | Close GitHub **#1** with summary + links to captures/commits | Acceptance checklist done |

### Out of scope for #1 (do not block)

- Full librtlsdr gain matrix day one  
- ESP32-S2/S3  
- Non–Blog V4 sticks  
- Claiming cal-lab absolute dBm from TinySA  
- Completing #2 multi-hour soak (parallel track)  

---

## 6. Implementation touchpoints (code map)

| Area | Where |
|---|---|
| Public API stubs | `include/esp_rtl_sdr.h` (gain/bias section), `src/esp_rtl_sdr.cpp` setters |
| CAP bits | `esp_rtl_sdr_get_capabilities()` in `src/esp_rtl_sdr_policy.cpp` |
| CAP must stay off until ready | `tests/scripts/check_truth_hygiene.sh` + host tests |
| Blog V4 EP0 / tables | `private/transfers_blog_v4.hpp` (and related) |
| Init/tune already measured | Reuse I2C/control helpers; **add** gain/bias sequences from **new** captures only |

**Clean-room order:** our captures → DS/register PDFs for naming → behavior of desktop apps as black box → **never** copy librtlsdr source.

---

## 7. Evidence package template

```text
docs/captures/   (or C:\Tools\esp-rtl-sdr-lab\captures\ then copy)
  bias_on_off.pcapng
  gain_steps.pcapng
  NOTES.md
```

**NOTES.md minimum fields:**

```text
Date:
Operator:
PC OS / Wireshark / USBPcap / Npcap versions:
rtl_biast path / Blog release tag:
SDR# or SDR++ version:
Dongle: Blog V4 serial (redact middle if public):
VID:PID: 0bda:2838
Bias procedure: steps + times
Gain procedure: LO, rate, UI dB steps + times
Multimeter bias ON (V):
TinySA used? (yes/no + settings)
SHA-256:
  bias_on_off.pcapng = ...
  gain_steps.pcapng = ...
Honesty: black-box stimulus only; implement from capture
```

---

## 8. Risks and anti-patterns

| Risk | Mitigation |
|---|---|
| Enable CAP without capture | Forbidden; CI + PROJECT_TRUTH |
| Paste librtlsdr gain tables | CLEAN_ROOM; reject in review |
| USBPcap missing after reboot | Reinstall USBPcap / check driver; `tshark -D` |
| Zadig bricks wrong interface | Only Bulk-In interface 0 for RTL tools |
| Bias-T damages gear | Multimeter; no DC into TinySA RF; know antenna DC path |
| Capture after app already open | Always start Wireshark **before** open |
| Confuse TinySA USB with RF | RF is SMA only |

---

## 9. Definition of done for #1

- [ ] Bias + gain pcapng (+ NOTES + hashes) exist and are referenced  
- [ ] Measured sequences in profile; `set_bias_tee` / gain APIs affect hardware  
- [ ] `CAP_BIAS_TEE` and/or `CAP_GAIN` on **only** for implemented paths  
- [ ] Host tests + hygiene CI green  
- [ ] PROJECT_TRUTH updated with correct evidence labels  
- [ ] Issue #1 closed with links  

**Not required for #1 close:** 24 h P4 soak (#2), TinySA cal, multi-dongle support.

---

## 10. Related links

| Resource | URL / path |
|---|---|
| Issue #1 | https://github.com/hardcoreerik/esp-rtl-sdr/issues/1 |
| Repo | https://github.com/hardcoreerik/esp-rtl-sdr |
| Capture procedure | `docs/GAIN_BIAS_CAPTURE.md` |
| PC setup | `docs/PHASE3_PC_SETUP.md` |
| Lab tools | `C:\Tools\esp-rtl-sdr-lab\README.md` |
| Clean-room | `docs/CLEAN_ROOM.md` |
| Truth | `PROJECT_TRUTH.md` |
| Roadmap Phase 3 | `Roadmap.md` |

---

## 11. Suggested first message after paste

> Verify post-reboot: `tshark -D` and `rtl_test -t` with Blog V4 on PC. If USBPcap and dongle OK, run Phase B bias capture end-to-end and stage files under `C:\Tools\esp-rtl-sdr-lab\captures\` with NOTES.md. Do not enable CAP yet.

---

*End of handoff. Update this file when #1 advances (capture done / CAP on / issue closed).*
