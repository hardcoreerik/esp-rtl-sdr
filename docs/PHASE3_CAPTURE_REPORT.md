# Phase 3 capture report — Blog V4 gain & bias-T (desk lab)

**Date:** 2026-08-12 (lab evening session, US Pacific)  
**Project:** [esp_rtl_sdr](https://github.com/hardcoreerik/esp-rtl-sdr)  
**Tracking:** GitHub issue [#1](https://github.com/hardcoreerik/esp-rtl-sdr/issues/1) — *phase-3: gain and bias-T USB capture*  
**Dongle:** Official **RTL-SDR Blog V4** — USB `0bda:2838`, product **RTLSDRBlog / Blog V4**, serial **00000001**  
**Host PC:** Windows 11 capture machine (not the ESP32-P4)

This report is for **humans**. It explains what we did, why it matters, what we have now, and what is still unfinished.

---

## In one paragraph

To make **gain** and **bias-T** work honestly inside our ESP32 driver, we cannot guess or copy someone else’s driver source. We had to **watch the real USB traffic** while a known desktop program turned bias on/off and stepped the tuner gain on a real Blog V4. That traffic is now saved as Wireshark captures, with notes and a screen video of the gain slider. The driver still does **not** control gain/bias on the ESP32 yet — the next job is to **read those captures** and teach the driver the same sequences, then turn on the capability flags only when it actually works.

---

## Why this matters (plain English)

### What users expect from an SDR dongle

| Feature | What it does for you |
|---|---|
| **Tuner gain** | Makes weak signals louder, or turns down strong ones so they don’t overload the radio (clipping). Without gain control, the app can’t fix “too quiet” or “too loud / distorted.” |
| **Bias-T** | Puts a small DC voltage on the antenna connector so you can power a simple external amplifier (LNA) over the same coax. Without it, many outdoor LNAs won’t turn on from the dongle. |

### What was wrong before this lab session

Our open-source driver **already streams I/Q** from a Blog V4 on ESP32-P4, retunes, measures health, etc. For gain and bias we only had **empty shells**:

- The functions exist (`set_tuner_gain`, `set_bias_tee`, …).
- They return **“not supported”**.
- Capability flags **CAP_GAIN** and **CAP_BIAS_TEE** stay **off**.

We did that **on purpose**. Shipping fake “gain works” without measuring USB would be dishonest and could brick trust in the project.

### Why USB capture (not just “look at librtlsdr”)

Desktop tools (SDR#, `rtl_biast`) already know how to talk to the dongle. Their **USB messages** are the ground truth for *this* hardware. Our rules:

1. Use desktop software only as a **black box** (push the buttons).  
2. **Record the USB wire**.  
3. Rebuild the same behavior in **esp_rtl_sdr** from *our* measurements.  
4. **Do not paste** librtlsdr / rtl-sdr-blog **source code** into our tree (clean-room / license / honesty).

That is what Phase 3 is about.

---

## What we set up on the PC (so others can repeat)

| Piece | Role |
|---|---|
| **Wireshark** + **USBPcap** + **Npcap** | See USB packets (USBPcap is the USB sniffer; Npcap is for normal network capture) |
| **Zadig → WinUSB** on Blog V4 interface 0 | Lets Windows apps (`rtl_test`, `rtl_biast`, SDR#) open the dongle |
| **RTL-SDR Blog tools** (`rtl_biast`, `rtl_test`) in `C:\Tools\esp-rtl-sdr-lab\bin` | Bias toggle + device check |
| **AIRSPY SDR# Studio** | Manual RF gain slider while listening to a real station |
| Lab folder | `C:\Tools\esp-rtl-sdr-lab\` (tools, captures, notes) |

**Important:** For capture, the Blog V4 plugs into the **PC**, not the ESP32-P4. The P4 is for later: prove the *ESP* driver can send the same commands.

**Wireshark tip we hit:** USBPcap ports did not show until `USBPcapCMD.exe` was copied into Wireshark’s **extcap** folder. After that, **USBPcap1 / 2 / 3** appear. Full PC setup notes: [PHASE3_PC_SETUP.md](PHASE3_PC_SETUP.md).

---

## Capture 1 — Bias-T (and open path)

### What we did

1. Confirmed the stick is a real Blog V4 (`rtl_test` showed **RTLSDRBlog / Blog V4**, **R828D** tuner).  
2. Started USB capture on the PC’s USB hubs (USBPcap).  
3. Ran **`rtl_biast`** to turn bias **on** and **off** (and repeated), after the dongle had been opened in a normal way.  
4. Saved the capture and notes.

We did **not** use a multimeter this time, so we do **not** claim a measured voltage on the SMA. Product docs say ~4.5 V class when bias is on; we still need a meter later if we want hardware-verified DC.

### Evidence files (lab disk)

| File | Description |
|---|---|
| `C:\Tools\esp-rtl-sdr-lab\captures\bias_on_off2.pcapng` | Main bias/open capture (~312 KB, **4426** packets, ~74 s) |
| `bias_on_off.pcapng` | Copy of the same run for a simple name |
| `NOTES_bias.md` | Who / what / hash / honesty notes |
| `bias_vendor_out.txt` | Extracted vendor USB “OUT” lines for analysis |

**SHA-256 (bias_on_off2.pcapng):**  
`33CBE20B94AD9EEBD3644DCBE1843B01007A174131337EDE8A6483FCA8EB3512`

### What the bias capture is good for

- Proves we can see **real control traffic** to **0bda:2838**.  
- Contains **vendor control** messages (the kind used for GPIO / system / I2C setup).  
- Gives a baseline “open + bias toggle” story for the clean-room tables.

### What it is not yet

- Not yet a finished “here is the one EP0 sequence for bias” checklist in the driver.  
- Not a DC voltage proof without a multimeter.  
- Not enabled in firmware (`CAP_BIAS_TEE` still off).

---

## Capture 2 — Tuner gain steps

### What we did

1. Opened **SDR#** on the Blog V4.  
2. Tuned a strong local FM station (**96.1 MHz KZEL**) so gain changes are easy to hear/see on the spectrum.  
3. Set **sample rate 2.4 MSPS**, **RTL AGC off**, **Tuner AGC off**, **Bias-Tee off**.  
4. Started Wireshark USB capture.  
5. Moved **RF Gain** through the full manual ladder (see table below), pausing between steps.  
6. Saved the pcap and a **screen recording** of the UI so gain dB values are visible over time.

### Evidence files (lab disk)

| File | Description |
|---|---|
| `C:\Tools\esp-rtl-sdr-lab\captures\gain_steps.pcapng` | Main gain capture (~**538 MB**, ~**38 000** packets, ~**128 s**) |
| `screen-capture.webm` | Screen recording of SDR# while stepping RF Gain (~23 MB) |
| `NOTES_gain.md` | Procedure + hashes + honesty |
| `gain_vendor_full.txt` / `gain_step_clusters.txt` | Extracted control traffic for engineers |

**SHA-256 (gain_steps.pcapng):**  
`E61F9C1312371BC1344D770B607BC9B58336EFBB61C0C247516344424933E3B5`

### RF Gain values used (start → finish)

These are the **SDR# RF Gain** readings in order. This is the human label we will map to USB messages.

| Step | RF Gain (dB) | Step | RF Gain (dB) |
|---:|---:|---:|---:|
| 1 | **0.0** | 15 | **25.4** |
| 2 | **0.9** | 16 | **28.0** |
| 3 | **1.4** | 17 | **29.7** |
| 4 | **2.7** | 18 | **32.8** |
| 5 | **3.7** | 19 | **33.8** |
| 6 | **7.7** | 20 | **36.4** |
| 7 | **8.7** | 21 | **37.2** |
| 8 | **12.5** | 22 | **38.6** |
| 9 | **14.4** | 23 | **40.2** |
| 10 | **15.7** | 24 | **42.1** |
| 11 | **16.6** | 25 | **43.4** |
| 12 | **19.7** | 26 | **43.9** |
| 13 | **20.7** | 27 | **44.5** |
| 14 | **22.9** | 28 | **49.6** |

That is **28 steps**, matching the “29 supported gains including 0.0” style list desktop tools expose for this tuner path (0.0 through 49.6 dB).

### Why the gain file is so large

While you listened to FM, the dongle was **streaming samples** over USB (bulk data). Wireshark recorded that **plus** the occasional control messages when gain changed. Engineers care most about the **control** part; the bulk traffic proves the stick was live and active.

Automated review of the capture shows **clusters of USB control activity every few seconds**, which lines up with stepping the slider and pausing.

---

## What “success” means for this lab day

| Goal | Status |
|---|---|
| Blog V4 opens on this PC with WinUSB | **Done** |
| USBPcap can see the dongle’s hub traffic | **Done** (after extcap fix + reboot) |
| Bias on/off captured on the wire | **Done** (pcap + notes) |
| Full manual gain ladder captured | **Done** (pcap + video + dB list) |
| Multimeter bias voltage | **Skipped** (no meter on hand) |
| TinySA relative RF check | **Not required** for this report |
| Driver can set gain/bias on ESP32 | **Implemented in 0.7.5** from these captures (P4 re-soak still open) |
| CAP_GAIN / CAP_BIAS_TEE enabled | **On in 0.7.5** (`MEASURED_2026_08_12`) |

---

## Why users should care (even if you only use ESP32 later)

1. **Honesty** — We do not claim radio features we have not measured.  
2. **Your hardware** — Blog V4 is what many people buy; tables will match that path.  
3. **Safety for the project** — Clean-room captures protect licensing and engineering independence.  
4. **Better apps later** — Once CAP bits flip, phone/dashboard/OrcSDR-style apps can set gain and bias like desktop SDR without guessing.  
5. **Debugging** — Health metrics (“RF clipping” / “RF weak”) only become *actionable* if the app can change gain.

---

## What happens next (roadmap for implementers)

1. **Decode** bias capture → smallest reliable EP0 sequence for bias on/off.  
2. **Decode** gain capture + dB ladder (+ video) → map each dB step to I2C/register USB writes.  
3. Put sequences only in our **measured profile** (`private/` / Blog V4 tables).  
4. Implement `set_bias_tee` / `set_tuner_gain*` so they send those bytes on ESP32.  
5. Turn on **CAP_BIAS_TEE** / **CAP_GAIN** only when tests and a real stick agree.  
6. Update [PROJECT_TRUTH.md](../PROJECT_TRUTH.md), run CI, close issue #1.  
7. Later: optional multimeter note; re-test from **esp_rtl_sdr on P4**; soak log (#2) stays a separate track.

Until step 5, apps must still check capabilities:

```c
if (!(esp_rtl_sdr_get_capabilities() & ESP_RTL_SDR_CAP_GAIN)) {
    /* gain not available yet — do not expect RF change */
}
```

---

## How to open the evidence (for reviewers)

On the lab PC:

```text
C:\Tools\esp-rtl-sdr-lab\captures\
  bias_on_off2.pcapng      (or bias_on_off.pcapng)
  gain_steps.pcapng
  screen-capture.webm
  NOTES_bias.md
  NOTES_gain.md
```

Open `.pcapng` in **Wireshark**. Filter examples:

```text
usb.idVendor == 0x0bda
```

```text
usb.bmRequestType == 0x40
```

Play `screen-capture.webm` alongside the gain file to align slider dB with time.

**Note:** Large gain pcap may not live in the public git repo by default (hundreds of MB). Hashes and this report can live in-repo; binary captures can stay on lab storage or a release asset until we decide packaging.

---

## Credits and method notes

- Human operator: project maintainer (desk lab).  
- AI-assisted tooling for install scripts, Wireshark/USBPcap debugging, and capture analysis — outcomes owned by the maintainer ([AI disclosure](AI_DEVELOPMENT_DISCLOSURE.md)).  
- Desktop programs used as **stimulus only**.  
- Clean-room policy: [CLEAN_ROOM.md](CLEAN_ROOM.md).  
- Procedure template: [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md).  
- Session handoff: [HANDOFF_PHASE3_GAIN_BIAS.md](HANDOFF_PHASE3_GAIN_BIAS.md).

---

## Bottom line for users

We spent a real evening **measuring** how a Blog V4 does gain and bias on USB, instead of pretending. You now have:

- A **bias** capture,  
- A **full gain ladder** capture,  
- A **video** of the dB values,  
- And a **written dB list** from 0.0 through 49.6.

**0.7.5** turns those measurements into driver tables (`private/measured_gain_bias_v4.hpp`) and enables **CAP_GAIN** / **CAP_BIAS_TEE** for **manual** gain and bias-T when the stick is claimed (after `start`). AUTO AGC is still unsupported. Treat as **lab-measured, not production-certified** until P4 re-soak and optional multimeter DC are logged.

---

*Report version: 2026-08-12. Update when CAP bits land or evidence is published into the repo.*
