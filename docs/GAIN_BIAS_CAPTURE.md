# Gain & bias-T capture procedure (Phase 3)

> **Status (0.7.5+):** CAP_GAIN and CAP_BIAS_TEE are **on** for Blog V4 manual gain
> and bias-T, from clean-room USBPcap tables in `private/measured_gain_bias_v4.hpp`.
> AUTO AGC remains unsupported. P4 re-soak of the ESP path is still open.
> This document is the **capture procedure** for new profiles / re-measurement.
> Do **not** paste librtlsdr / rtl-sdr-blog tables.

---

## Goal

Independently observe Blog V4 USB control traffic for:

1. **Tuner gain** — manual steps and/or auto mode (R828D via RTL2832 I2C)
2. **Bias-T** — GPIO path that enables ~4.5 V on the SMA (product DS claim)

Then implement only from those captures in this repo (clean-room).

---

## Gear (lab)

| Item | Role |
|---|---|
| RTL-SDR Blog V4 | DUT |
| Windows/Linux PC | Capture host — Wireshark + USBPcap / `usbmon` |
| ESP32-P4 optional | Later validate ESP path |
| **TinySA Ultra** | Relative RF / weak generator — **not** a substitute for USB capture |
| Baofeng UV-5R / Flipper / Heltec | RF stimulus (distance, low power) |
| Multimeter | Bias-T **DC** when enabled |

**Hobbyist desk is enough** for USB + multimeter + relative RF. See honest limits and
TinySA step-by-step: **[LAB_HOBBYIST.md](LAB_HOBBYIST.md)**.

**PC software setup (this maintainer desk):** **[PHASE3_PC_SETUP.md](PHASE3_PC_SETUP.md)**  
Tools live under `C:\Tools\esp-rtl-sdr-lab\` (Wireshark, USBPcap, `rtl_biast`, SDR#/SDR++, Zadig).  
For #1, plug the **Blog V4 into the PC**, not the ESP32-P4.

---

## Capture plan

### A. Bias-T

1. Install official Blog V4-compatible desktop tools that can toggle bias-T
   (or a known `rtl_biast` binary — use as **black-box stimulus only**).
2. Start USB capture **before** open.
3. Toggle bias ON, wait 2 s, toggle OFF.
4. Save capture + note VID/PID/serial/product strings.
5. Diff control transfers around toggle; extract EP0 patterns to SYS GPIO.
6. Measure SMA center conductor DC vs shield with multimeter (expect ~4.5 V class
   when ON — confirm against product DS, not assumptions).

### B. Gain

1. Capture with a desktop app that changes gain steps (manual) and AGC on/off.
2. Sweep several discrete gain settings at fixed LO (e.g. 100 MHz) and rate (960k).
3. Log: requested dB (app UI) ↔ USB control bytes.
4. Prefer **I2C write** patterns to the R828D address path already used in our
   init/tune tables (do not invent registers from memory).
5. Cross-check R820T2 public register description for *names* only after capture
   shows which registers change ([SILICON.md](SILICON.md)).
6. **Optional RF corroboration (TinySA Ultra):** fixed weak generator tone (or stable
   broadcast) while stepping gain; record only **relative** louder/quieter or ΔdB —
   not a cal table. Procedure in [LAB_HOBBYIST.md](LAB_HOBBYIST.md).
### C. Evidence package (check into `docs/captures/` or private vault + hash here)

| File | Content |
|---|---|
| `bias_on_off.pcap` / `.pcapng` | Raw capture |
| `gain_steps.pcap` | Raw capture |
| `NOTES.md` | Procedure, software versions, serial, hashes (SHA-256) |
| Optional photos | Multimeter bias ON |

PROJECT_TRUTH label after implement: **Implemented** then **Hardware-verified**
only when this tree drives the same EP0 and P4 stream still works.

---

## Implementation gate

| Step | Done when |
|---|---|
| Capture exists | Files + NOTES with hashes |
| Tables in profile | `private/` or profile module — measured only |
| API applies hardware | set_* returns ESP_OK and CAP bit set |
| Host tests | Still pass; optional new policy tests |
| Lab RF | Baofeng/Flipper show health clip/weak respond to gain |

**0.7.5 Blog V4 path** already meets the gate for **manual** gain + bias-T CAP bits.
Re-run this procedure for new dongles/profiles or when re-measuring.

```c
if (esp_rtl_sdr_get_capabilities() & ESP_RTL_SDR_CAP_GAIN) {
    /* after start: set_tuner_gain_mode(MANUAL) + set_tuner_gain(tenths) */
}
```

---

## Anti-patterns

- Copying gain tables from librtlsdr source  
- Enabling CAP_GAIN because “desktop does it” without our capture  
- Claiming bias-T DC works after only USB EP0 success (need multimeter for electrical claim)
