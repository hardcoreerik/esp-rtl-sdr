# AGC + IF-filter USB capture (Blog V4)

**Goal:** Independently observe Blog V4 USB control traffic for:

1. **Tuner AGC** on/off (R828D auto gain) — unblocks `set_tuner_gain_mode(AUTO)`
2. **RTL AGC** on/off (RTL2832 digital AGC) — separate path; capture so we do not mix it up
3. **IF / channel filter / tuner bandwidth** at a **fixed** sample rate — unblocks `CAP_IF_FILTER` *if* USB actually changes

**Not this session:** manual RF gain ladder (already measured 2026-08-12), bias-T, P4 streaming.

Desktop software is **black-box stimulus only**. Do **not** paste librtlsdr source. Clean-room: [CLEAN_ROOM.md](CLEAN_ROOM.md).

Same PC lab as Phase 3: `C:\Tools\esp-rtl-sdr-lab\`. Dongle plugs into **this PC**, not the ESP32-P4.

---

## What plugs where

| Thing | Where |
|---|---|
| RTL-SDR **Blog V4** | USB port on **this Windows PC** |
| ESP32-P4 / Tab5 / Waveshare | Unplug the dongle from it; leave it alone |
| Antenna | On the Blog V4 (FM broadcast is enough) |

---

## Software to open (only these)

| App | Path / how | Role |
|---|---|---|
| **PowerShell** | Windows Terminal / pwsh | `rtl_test`, later notes |
| **Wireshark** | Start menu, or `C:\Program Files\Wireshark\Wireshark.exe` | USB sniffer |
| **SDR# (AIRSPY SDR# Studio)** | `C:\Tools\esp-rtl-sdr-lab\bin\SDRSharp.exe` | Click AGC / filter |
| **Zadig** | `C:\Tools\esp-rtl-sdr-lab\bin\zadig.exe` | **Only if** `rtl_test` cannot open the stick |

Do **not** open OrcSDR, rtl_tcp, a second SDR#, or SDR++ at the same time. One program owns the dongle.

---

## Prep (do this first — no capture yet)

1. Unplug the Blog V4 from the P4. Plug it into the **PC**.
2. Close anything that might be using it.
3. Open a **new** PowerShell:

```powershell
cd C:\Tools\esp-rtl-sdr-lab
$env:Path = "C:\Tools\esp-rtl-sdr-lab\bin;C:\Program Files\Wireshark;" + $env:Path
rtl_test -t
tshark -D
```

4. **Good `rtl_test`:** you see **RTLSDRBlog / Blog V4**, USB `0bda:2838`, tuner **R828D**. Ctrl+C to stop.  
   **Bad (`usb_open error`, nothing listed):** run Zadig as admin → Options → List All Devices → Bulk-In **Interface 0** (`0BDA 2838`) → WinUSB → retry `rtl_test -t`.
5. **Good `tshark -D`:** lines named **USBPcap1**, **USBPcap2**, …  
   If those names are missing, USBPcap is not loaded — reboot once, then try again.
6. Do **not** start SDR# until Wireshark is capturing (see below).

---

## Wireshark: start capture

1. Open **Wireshark**.
2. On the start screen, hold **Ctrl** and click **USBPcap1**, **USBPcap2**, and **USBPcap3** (same trick as the gain session — we may not know which hub).
3. Click the blue shark fin (**Start capturing**).
4. Display filter (optional, after start): `usb`

Keep capture **short**. Start → do the clicks → stop. A 2-minute FM sit makes a huge file of IQ bulk we do not need.

---

## Capture A — Tuner AGC (the important one)

Save as: `C:\Tools\esp-rtl-sdr-lab\captures\agc_tuner_on_off.pcapng`

1. Wireshark already capturing.
2. Open **SDR#**. Source = RTL-SDR USB. Play (triangle).
3. Tune **96.1 MHz** (KZEL, same as last time). Sample rate **2.4 MSPS**.
4. On the **RTL-SDR controller** panel (source configure — **not** the audio/radio AGC):
   - **RTL AGC** = **off**
   - **Tuner AGC** = **off**
   - **Bias-Tee** = **off**
   - RF Gain = a mid manual value (e.g. ~30 dB) and **leave it**
5. Wait 2 seconds.
6. Check **Tuner AGC** **ON**. Wait 3 seconds.
7. Uncheck **Tuner AGC** (back **OFF**). Wait 3 seconds.
8. Repeat ON / wait / OFF / wait **one more time**.
9. Stop Play. **Stop** Wireshark. File → Save As the name above.

Write wall-clock times in `NOTES_agc.md` if you can (“Tuner AGC ON at ~0:12”).

**Do not confuse:** SDR# also has an **audio** AGC in the radio panel (`agcEnabled` in config). Ignore it. We only care about **Tuner AGC** on the RTL-SDR source panel.

---

## Capture B — RTL AGC (cheap extra)

Save as: `C:\Tools\esp-rtl-sdr-lab\captures\agc_rtl_on_off.pcapng`

Same setup. **Tuner AGC stays OFF.** Toggle only **RTL AGC** off → on → off (twice). Stop and save.

This is a different chip block. If we skip it, we might later mistake RTL digital AGC for tuner AGC.

---

## Capture C — Filter / IF bandwidth (may be silent — that is still data)

Save as: `C:\Tools\esp-rtl-sdr-lab\captures\if_filter_steps.pcapng`

1. New Wireshark capture (USBPcap1–3 again).
2. SDR# playing: 96.1 MHz, **2.4 MSPS**, Tuner AGC **off**, RTL AGC **off**, Bias **off**, RF Gain **fixed**.
3. Mode **WFM**.
4. On the radio panel, change **Filter** (or bandwidth) slowly, pausing ~2 s each:

   `80 kHz → 100 → 120 → 150 → 180 → 200 → 220 kHz`

   (Use whatever steps SDR# actually shows. Pause between them.)
5. **Do not** change sample rate in this capture. Rate already has its own EP0.
6. Stop Play. Stop Wireshark. Save as above.

**Honesty:** SDR# “Filter” is often **software after IQ**. USB may not change. That means `CAP_IF_FILTER` stays **off** until we find a control that really hits the R828D. Capture anyway.

Optional if Filter produced no obvious vendor traffic (we will check): one extra run in **SDR++** (`C:\Tools\esp-rtl-sdr-lab\bin\sdrpp.exe`) looking for a **Tuner Bandwidth** control, same fixed rate. Save `if_filter_sdrpp.pcapng`.

---

## After you save

Tell the session:

- File names + that they exist
- Whether Tuner AGC / RTL AGC / Filter actually moved in the UI
- `rtl_test` identity if you still have the window

Then we hash the files, extract vendor EP0 (`bmRequestType 0x40`) like Phase 3, and only then implement AUTO / IF.

---

## Do not

- Leave capture running while you listen to the radio
- Toggle RF Gain in these files (contaminates AGC)
- Copy registers from GitHub
- Claim AUTO works because desktop does

---

## Lab result (2026-08-26)

Dongle: Blog V4 `0bda:2838` SN 00000001. SDR#: 96.1 MHz, 2.4 MSPS, RF Gain 29.7 dB parked.

| File | SHA-256 | Result |
|---|---|---|
| `agc_tuner_on_off.pcapng` | `E131C5C642E7DFE8443D4667975871C220FFF4A237826B8423E9E38944BD3E8E` | 4 clusters: AUTO ON `05=E8 07=78 0C=6B` |
| `agc_rtl_on_off.pcapng` | `1E5B006179C548E62617F530D5BC292AD45900763F6BCEF357264D4FDD55D482` | 4 writes: demod `0x19` ON=`0x25` OFF=`0x05` |
| `if_filters_steps.pcapng` | `D9D32608418D40A48E88C925E804C7D1B35AA782A604EEC02CC90C60AA27A9FB` | **No vendor OUT after open** |

Shipped in **0.7.8** as `CAP_GAIN_AUTO` + `CAP_RTL_AGC`. No `CAP_IF_FILTER`.
