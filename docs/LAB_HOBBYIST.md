# Hobbyist lab — honest capabilities & how to use what we have

> This is **not** a calibrated RF chamber, a $50k Keysight bench, or a professional
> EMC lab. It is one hobbyist’s desk, a handful of radios, and careful notes.
> That is enough for **useful** evidence if we stay honest about uncertainty.

**Authoritative product truth still lives in** [PROJECT_TRUTH.md](../PROJECT_TRUTH.md).  
**Automated software tests:** [TESTING_GUIDE.md](TESTING_GUIDE.md).  
**Phase 3 capture gates:** [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md).

---

## What this lab is

| Is | Is not |
|---|---|
| A **real** Blog V4 + ESP32-P4 path we already used in apps | NIST-traceable measurements |
| Good enough to **see** signals, gain steps *roughly*, USB control chatter | “Exact dB at antenna terminal” claims |
| Good enough to **prove** bias-T DC came on (multimeter) | Professional SAR / antenna pattern work |
| Good enough to log **before/after** with a TinySA Ultra | Replacing a USB protocol analyzer ($thousands) |
| Legal only for **licensed / Part 15 / no-harm** TX | Permission to blast HT power into the dongle |

If a claim needs a cal lab, we mark it **Planned** or **Provenance** — not **Hardware-verified**.

---

## Inventory (update when gear changes)

### Hosts / DUT

| Device | Role | Honesty note |
|---|---|---|
| RTL-SDR **Blog V4** | Main DUT | Official stick; our profile target |
| ESP32-P4 **Tab5** / **Waveshare** | Embedded host | Provenance stream under OrcSDR; re-soak from this tree still open |
| Windows PC | USB capture + desktop SDR apps | USBPcap quality varies; not a Lecroy |

### RF / tools we actually own

| Device | Best use for *this* project | Limits |
|---|---|---|
| **TinySA Ultra** | See RF present; compare relative level; use built-in **generator** for weak controlled tones | Not a lab SA; input damage risk; absolute dBm is *approximate* |
| **2× Heltec V4** | Controlled LoRa packets (prior decode work) | Band/region rules; not a clean CW synthesizer |
| **Baofeng UV-5R** | Strong nearby FM/voice; “front-end alive?” | High TX power — **danger** to front-ends; legal limits |
| **Flipper Zero** | Sub-GHz tones / protocols / interferer | Low power, limited bands; hobby accuracy |
| Multimeter (if available) | Bias-T **DC** on SMA | Not RF power |

### Software (typical hobbyist stack)

| Tool | Role |
|---|---|
| Wireshark + **USBPcap** (Windows) or `usbmon` (Linux) | USB EP0 capture for gain/bias |
| SDR# / SDR++ / GQRX + Blog V4–capable drivers | Black-box stimulus (open, retune, gain, bias) — **not** source to copy |
| `rtl_biast` (blog release) | Bias toggle stimulus only |
| Phone photos + text notes | Evidence when formal logs are messy |

---

## What we can prove with this lab (honest tiers)

### Tier A — Strong enough for PROJECT_TRUTH “Implemented / Hardware-verified” *if logged*

| Claim | How |
|---|---|
| Bias-T toggles **DC** on SMA | Multimeter + USB capture of toggle; photo optional |
| Gain UI changes **something** on USB | Capture ON gain-up / gain-down; note app dB labels |
| Stream lives on P4 | Smoke / passport logs from *this* repo |
| Relative RF louder/quieter after gain change | TinySA marker ΔdB or SDR# SNR/S-meter *delta* (not absolute) |

### Tier B — Useful, but label **Provenance** or “lab note”

| Claim | Why weaker |
|---|---|
| “Exact” dB gain table | No cal standards, cable loss unknown |
| Antenna performance | Desk multipath, no chamber |
| Absolute noise figure | Needs proper NF setup (TinySA *can* attempt; treat as exploratory) |

### Tier C — Out of scope for this lab

- Formal EMI certification  
- Accurate EIRP / illegal power claims  
- Guaranteeing every RTL2832U clone  
- Using TinySA as the only truth for USB register maps  

---

## Safety rules (read before TX)

1. **Never** key a Baofeng at full power into a Blog V4 SMA with a short coax and no attenuator. You can fry the front-end.  
2. Prefer: **distance** (room away), **very low power**, rubber duck not hard-line, or Flipper/Heltec low-power first.  
3. TinySA Ultra: respect **max input** (check your unit’s sticker/manual — typically keep signals well below +10 dBm; when unsure use antenna coupling, not direct HT→SA).  
4. Bias-T: do not power an antenna that DC-shorts the SMA; use a DC block if probing with TinySA while bias might be on.  
5. **Legal**: only transmit on frequencies/power you are allowed to use where you live.

---

## TinySA Ultra — how to use it *here*

Official docs: [tinysa.org](https://tinysa.org/) · Ultra notes: [tinysa.org/ultra](https://tinysa.org/ultra/).  
UI is touchscreen; exact menu names can vary slightly by firmware — if a path differs, use **FREQ / LEVEL / MARKER / MODE** equivalents.

### What TinySA Ultra is good for in our lab

| Job | TinySA mode |
|---|---|
| “Is there RF at ~100 MHz / 162 MHz / 433 MHz?” | Spectrum analyzer, center + span |
| “Did gain step make the peak ~taller?” | Marker peak, note dB *delta* only |
| Weak controlled test tone without Baofeng | **Signal generator** (OUT) → couple loosely to Blog V4 antenna |
| Rough birdies / birdie hunt | Wide span, then zoom |

### Mode A — Spectrum analyzer (look at the world or a TX)

1. Power on; leave **Ultra** mode available if you need GHz (firmware dependent).  
2. Connect antenna or cable to **RF in** (not OUT).  
3. Set **center frequency** (examples below).  
4. Set **span** wide first (e.g. 20–100 MHz), then narrow when you find energy.  
5. Adjust **RBW** if available: wider = faster/noisier; narrower = slower/cleaner.  
6. Place a **marker** on the peak → note **marker frequency** and **level**.  
7. Change only **one** variable (e.g. SDR gain step, or move HT farther) → record ΔdB.

**Starter center frequencies for our gear**

| Target | TinySA center (approx) | Span start |
|---|---|---|
| FM broadcast (NEED_FM) | 98 MHz | 20 MHz |
| NOAA WX | 162.4 MHz | 5 MHz |
| ADS-B (1090) | 1090 MHz | 10 MHz (Ultra) |
| Heltec LoRa (region-dependent) | 433 / 868 / 915 MHz | 2–10 MHz |
| Baofeng 2 m / 70 cm | 146 / 446 MHz | 5 MHz |

### Mode B — Built-in signal generator (safer than HT)

Use this for **controlled, weak** RF when testing “front-end hears something” without a loud HT.

1. Switch to **generator** / **signal generator** mode (menu varies by firmware).  
2. Set frequency (start with **100 MHz** or **146 MHz**).  
3. Set level **as low as the UI allows** first.  
4. Connect: TinySA **OUT** → short coax → **optional attenuator** → either  
   - a small antenna near the Blog V4 antenna (**preferred**, no hard DC path), or  
   - direct to Blog V4 SMA only with strong attenuation and **bias-T off**.  
5. On the SDR: tune to same frequency, fixed rate (960k), watch spectrum/audio/metrics.  
6. Raise generator level slowly until you see energy — stop well before clipping.

**Honesty:** TinySA generator level is good for **relative** tests, not “calibrated field strength.”

### Mode C — Watching Blog V4 *receive* path (indirect)

You cannot “see” USB gain registers on the TinySA. You can only see:

- Whether a tone **exists** in the air/cable  
- Whether the **received** strength (on TinySA near the antenna, or on SDR UI) moves when gain changes  

Combined experiment (good hobbyist evidence):

```text
TinySA gen @ 100 MHz (low)  →  couple to Blog V4
Desktop SDR: fixed LO 100 MHz, manual gain steps 0 → mid → high
Log: app gain label + SDR# SNR or sample_max/min from our metrics + optional TinySA marker if probing TX leakage
USB: Wireshark capture in parallel for Phase 3 EP0
```

### TinySA + bias-T warning

If Blog V4 **bias-T is ON**, SMA center has **~4.5 V DC**.  
**Do not** connect TinySA RF port directly without knowing your cable/adapters and using a **DC block** if DC might reach the SA. Prefer multimeter for DC, TinySA for RF only with DC blocked.

---

## Using the other toys honestly

### Baofeng UV-5R

| Do | Don’t |
|---|---|
| Low power, far across the room, rubber duck | Full power into coax into Blog V4 or TinySA |
| Quick “is FM demod alive?” check | Call it a calibrated signal source |
| Monitor-only on public broadcast first | TX on restricted services |

### Flipper Zero

| Do | Don’t |
|---|---|
| Sub-GHz test tones / protocols as weak stimulus | Expect GHz or high power |
| Controlled interferer for health RF_WEAK/CLIP experiments | Treat as lab generator with known dBm |

### Heltec V4 ×2

| Do | Don’t |
|---|---|
| LoRa packet air tests (you already used for decode work) | Assume it validates R828D gain tables |
| Known on-air occupancy near ISM | Point high-duty TX into the stick at zero distance |

---

## Phase 3 lab path (what to do *this week* with what we own)

Order matters: **USB first**, RF second.

### Step 1 — USB capture setup (Windows hobbyist)

1. Install **Wireshark** + **USBPcap**.  
2. Plug Blog V4 into the PC (not into P4 for this capture).  
3. Start capture on the USBPcap device **before** opening the SDR app.  
4. Note: device serial, Windows driver (WinUSB via Zadig if needed for some tools).

### Step 2 — Bias-T capture (Tier A if multimeter confirms)

1. Start USB capture.  
2. Run `rtl_biast` or SDR++ bias checkbox (black-box).  
3. Bias ON → wait → OFF.  
4. Stop capture; save `bias_on_off.pcapng`.  
5. Multimeter: SMA center to shell, bias ON (~4–5 V class) / OFF (~0). Photo optional.  
6. Write `NOTES.md` (software version, serial, times).

TinySA: **not required** for bias DC; optional later if LNA-powered antenna radiates.

### Step 3 — Gain capture (Tier A for USB deltas)

1. New capture.  
2. Open SDR# / SDR++ with Blog V4 driver; set **manual gain**.  
3. Fixed frequency (e.g. 100 MHz or a strong local FM).  
4. Step gain **low → mid → high** slowly; pause 2 s each; note UI dB.  
5. Toggle AGC on/off once.  
6. Save `gain_steps.pcapng` + notes.

### Step 4 — RF sanity with TinySA (Tier B relative)

1. TinySA generator low @ same LO as SDR **or** listen to broadcast FM.  
2. At two gain settings, record:  
   - app gain label  
   - our `get_metrics` sample_min/max if on P4  
   - TinySA marker level only if measuring a **TX** source, not guessing RX dBm  
3. Expect: higher gain → larger sample swing / louder audio; not a specific dB number.

### Step 5 — Hand package to the driver project

See [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md) evidence package.  
We implement **only** from capture; TinySA notes go in the same NOTES as supporting RF story.

---

## Desk setup sketch

```text
[Baofeng / Flipper / Heltec] --air (distance!)--+
                                                 |
[TinySA Ultra]  RF-IN antenna   (watch spectrum) |
[TinySA Ultra]  GEN-OUT --atten?-- couple         |
                                                 v
                                            [Blog V4]----USB----[PC Wireshark]
                                                 |
                                            or USB Host
                                                 v
                                            [ESP32-P4]
```

Messy cables and multipath are **normal**. Write them down; don’t pretend they aren’t there.

---

## Logging template (copy into NOTES.md)

```text
Date:
Operator: hobbyist desk (not cal lab)
DUT serial:
PC OS / Wireshark / USBPcap versions:
Desktop SDR app + version:
TinySA Ultra firmware (if used):

Experiment:
Goal:

Setup (distance, antennas, attenuators, bias on/off):

Steps:
1.
2.

Results:
- USB file:  sha256:
- Multimeter bias ON/OFF:
- TinySA: center / span / marker f / marker level / gen level:
- Observations (clip, weak, audio, crashes):

What we claim:
What we do NOT claim:
```

---

## Bottom line

| Question | Answer |
|---|---|
| Can we do Phase 3 without a pro lab? | **Yes for USB capture + rough RF checks** |
| Will TinySA replace USB capture? | **No** |
| Will TinySA help prove gain *mattered*? | **Yes, relatively** (ΔdB / “louder”) |
| Is this “professional validation”? | **No — hobbyist evidence with labels** |
| What’s the highest-value next hour? | USBPcap bias toggle + multimeter, then gain steps capture |

When captures exist, we implement CAP_GAIN / CAP_BIAS_TEE from **those files**, not from memory or librtlsdr.
