# esp_rtl_sdr gaps vs desktop SDR# / librtlsdr-class tools

**Context:** Waveshare FM web path + Blog V4 measured gain/bias (0.7.6).  
**Purpose:** Track what desktop apps expose that this driver does **not** (or only partially) support — so app-layer SDR# mirrors stay honest.

Labels:

| Tag | Meaning |
|---|---|
| **App** | Can implement in OrcSDR / host without new EP0 |
| **Driver** | Needs public API + clean-room / measured EP0 |
| **HW** | Needs silicon path we do not claim yet |
| **Open** | Not started |

---

## A. RF front-end (RTL2832 + R828D / Blog V4)

| Desktop control | SDR# / rtlsdr | esp_rtl_sdr today | Gap owner |
|---|---|---|---|
| Manual tuner gain ladder | Full list | **Measured 28-step** Blog V4 (0.7.5+) | — done (measured) |
| Tuner AGC (auto) | Yes | **UNSUPPORTED** | **Driver** — need AGC EP0 capture |
| IF / channel filter (tuner) | Often via gain stages + IF filter | **No** — only software LPF after IQ | **Driver/HW** — R828D IF/filter regs not measured as CAP |
| Bias-T | Yes | **Measured SYS EP0** | Optional multimeter DC claim still open |
| Direct sampling / HF | Forks | **Not claimed** | **Driver/HW** |
| Offset tuning | Common | **No API** | **Driver** (policy + EP0 if needed) |
| Bandwidth / sample rate | Many rates | Continuous windows + quantize | App wires UI; driver OK |
| PPM / freq correction | Yes | Software ppm API | App: not on Waveshare web yet |
| Multi-device | Yes | Enumerate / select | App |
| USB reset / reinit | Yes | install/uninstall/reset | App |

### Driver follow-ups exposed by FM work

1. **Hardware channel filter** — SDR# “Filter” on WFM is partly **post-IQ DSP** (we mirrored that in `fm_pcm`) and partly **tuner IF bandwidth**. We never measured R828D IF-filter EP0 for Blog V4; software IF is the only honest CAP today.
2. **AUTO gain** — CAP_GAIN is manual only; `set_tuner_gain_mode(AUTO)` fails closed.
3. **Gain under bias / mid-stream** — improved by async bulk-pause EP0 (0.7.6); still not multimeter-certified bias DC.
4. **No stereo / RDS path in driver** — pure app/DSP (or future IQ streaming for host demod).

---

## B. FM demod / audio (desktop radio UI)

| Control | Desktop | OrcSDR Waveshare FM | Notes |
|---|---|---|---|
| Channel filter BW | 80…220+ kHz steps | **Software** 80/100/120/150/180/200/220 kHz | App DSP, not tuner EP0 |
| De-emphasis 50/75/off | Yes | **Yes** (app) | |
| Audio AGC | Yes | **Yes** (app) | |
| Squelch | Yes | **Yes** (app, RF dBFS) | |
| Stereo (pilot) | Yes | **No** | App DSP gap; CPU heavy on P4 |
| RDS | Optional | **No** | App |
| Snap to station / band plan | UI | Web presets only | App |
| Recording | Yes | Not in Waveshare shell | App |

---

## C. Delivery / host integration

| Capability | Desktop | Driver | Gap |
|---|---|---|---|
| Async IQ callback | Yes | EVT_IQ_BLOCK | — |
| Sync read | Yes | `read()` + delivery modes | — |
| rtl_tcp | Common | **Not in driver** | Separate product / App |
| IQ over network | rtl_tcp / SpyServer | Web PCM only (demod on MCU) | App choice |

---

## D. Priority backlog (honest)

### Driver (esp_rtl_sdr)

1. **AUTO AGC EP0** — clean-room capture + CAP path  
2. **Optional IF-filter / bandwidth EP0** for R828D if measured (do not invent)  
3. **Hardening** gain+bias under load (retries exist; P4 soak log)  
4. **Bias DC evidence** when multimeter available  

### App (OrcSDR_Waveshare)

1. ~~Channel filter / deemph / squelch / AGC~~ (in progress on `/fm`)  
2. Web **ppm** + sample-rate controls  
3. Stereo pilot (optional)  
4. Touch 320×240: show filter/gain status only (no full SDR# UI)  

---

## E. Rule

**Do not claim SDR# parity.** Mirror **controls** where we have measured EP0 or pure software DSP; leave CAP off for unmeasured hardware paths.
