# Test lab & RF fixtures

**Automated tests:** [TESTING_GUIDE.md](TESTING_GUIDE.md)  
**Honest hobbyist lab + TinySA Ultra how-to:** [LAB_HOBBYIST.md](LAB_HOBBYIST.md)  
**Phase 3 USB capture gates:** [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md)

---

## Lab posture (truth)

This project’s physical lab is a **hobbyist desk**, not a calibrated RF chamber.
That is enough for real evidence if every claim carries an evidence label
([PROJECT_TRUTH.md](../PROJECT_TRUTH.md)).

| We can | We should not claim |
|---|---|
| USB EP0 captures for bias/gain stimulus | NIST-traceable dB tables |
| Multimeter proof of bias-T DC | “Professional EMI certified” |
| TinySA Ultra relative RF levels / gen tones | Absolute field strength |
| P4 + Blog V4 stream smoke | Every eBay RTL stick works |
| Heltec / Flipper / Baofeng as **stimulus** | Safe to hard-line HT full power into SMA |

---

## Hosts (SDR under test)

| Host | Role |
|---|---|
| ESP32-P4 M5Stack Tab5 | Provenance continuous IQ (OrcSDR) |
| ESP32-P4 Waveshare Module-DEV-KIT | Second-board Blog V4 (OrcSDR shell) |
| Stand-alone `examples/p4_serial_smoke` | Driver-only re-soak (open) |

## RTL-SDR under test

| Device | Notes |
|---|---|
| RTL-SDR Blog V4 (`0bda:2838`) | Primary profile; R828D |

## Companion / observation gear

| Device | Role | Limits |
|---|---|---|
| **TinySA Ultra** | Spectrum view + weak **signal generator**; relative ΔdB | Not a lab SA; watch max input; use DC block if bias-T may be on |
| **2× Heltec V4** | LoRa packets (prior decode work); controlled ISM activity | Region/band rules |
| **Baofeng UV-5R** | Strong FM/voice stimulus; “front-end alive?” | **High power risk** to dongle/TinySA; legal TX only; distance/low power |
| **Flipper Zero** | Sub-GHz tones / interferer | Low power, limited bands |
| Multimeter | Bias-T DC on SMA | Not RF power |
| PC + USBPcap/Wireshark | Phase 3 USB capture | Hobbyist capture quality |

---

## Suggested exercises

| Goal | Method | Evidence strength |
|---|---|---|
| Rate passport | `probe_rates` on P4 + Blog V4 | Strong if logged from *this* tree |
| Continuous rate | `set_sample_rate` + stream | Strong on P4 |
| NEED_ADSB / FM / WX | `apply_need` + start | App-level; RF may be weak indoors |
| Health RF_WEAK / CLIP | Flipper/Baofeng distance change | Qualitative |
| Bias-T Phase 3 | USB capture + multimeter | **Strong** if both present |
| Gain Phase 3 | USB capture of manual steps + TinySA/SNR Δ | Strong USB; RF relative only |
| TinySA gen into Blog V4 | Low-level gen, couple by air | Safer than HT; relative |

Full TinySA menu walkthrough and desk sketch: **[LAB_HOBBYIST.md](LAB_HOBBYIST.md)**.

---

## Safety (short)

- Do not hard-line Baofeng full power into Blog V4 or TinySA.  
- Prefer air coupling, distance, low power, attenuators.  
- Bias-T ON ≈ DC on SMA — DC-block before SA RF ports.  
- Transmit only where legal.

---

## Related

- [LAB_HOBBYIST.md](LAB_HOBBYIST.md) — capabilities honesty + TinySA how-to  
- [GAIN_BIAS_CAPTURE.md](GAIN_BIAS_CAPTURE.md) — Phase 3 procedure  
- [TESTING_GUIDE.md](TESTING_GUIDE.md) — CI / host unit tests  
- [PROJECT_TRUTH.md](../PROJECT_TRUTH.md)  
