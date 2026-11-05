# Hardware scope (intentional, not accidental)

This project **fails closed** on unsupported hardware. Narrow support is a
**design choice**, not an unfinished port of librtlsdr.

---

## Implemented profiles and measured support

| Piece | Status | Notes |
|---|---|---|
| **MCU** | ESP32-**P4** High-Speed USB Host | e.g. M5Stack Tab5, Waveshare P4 kits |
| **Dongle** | RTL-SDR **Blog V4** | Primary tested receiver; exact `RTLSDRBlog` / `Blog V4` identity. |
| **Blog V4L** | Implemented / hardware-tested | Distinct V4 Lite tuner/HF route; Tab5 tuning, streaming and RF reception records exist. |
| **Nooelec** | NESDR SMArt V5 implemented / hardware-tested | Exact descriptor profile; P4 streaming/control accepted and 7.074 MHz FT8 decoded through OrcSDR; calibration and full-band coverage remain separate. See [capture record](captures/nooelec_v5_2026-09-30.md). |
| **E4000 / Nooelec XTR** | Implemented / initial P4 FM/RDS RF-validated | 64 MHz–1.7 GHz E4000 zero-IF path, 14 manual gain steps and tuner AUTO implemented. Initial Tab5 2.4 MS/s FM/RDS acceptance is recorded; extended hardware and long-soak qualification remains open. |
| **V3c** | Implemented / hardware-tested | Native tuning, streaming, hotplug and HF Q route implemented; gain calibration remains separate. Earlier V3 hardware variants require individual evidence. |
| **IQ** | Continuous CU8 multi-URB | CAP_STREAM |
| **Tooling** | ESP-IDF ≥ 5.5, `esp32p4` | CI builds smoke on 5.5.4 |

Evidence labels: [`../PROJECT_TRUTH.md`](../PROJECT_TRUTH.md). E4000 implementation details: [E4000 profile](E4000_PROFILE.md).

---

## Explicitly not claimed (yet)

| Piece | Label | Why deferred |
|---|---|---|
| ESP32-S2/S3 Full-Speed USB Host | **Deferred** | FS bulk economics differ; lower continuous SPS; needs separate URB + soak story |
| Generic RTL2832U / R820T sticks | **Deferred** | Identity + EP0 sequences not measured in *this* clean-room profile |
| librtlsdr ABI drop-in | **Deferred** | Never a goal — different API and license gravity |
| Production warranty | **None** | 0.x |

---

## Why P4 + Blog V4 first

1. **Host USB is the hard problem** on MCU — HS host on P4 is the fair fight.  
2. **One measured profile** beats “supports every eBay dongle” marketing.  
3. Blog V4 has a known product identity and prior provenance path under OrcSDR.  
4. Passport/health only mean something when *this* board + stick is characterized.

Expanding hosts/dongles is welcome **after** stand-alone P4 soak and Phase 3
gain/bias capture — see Roadmap and open issues labeled `scope` / `port`.

---

## Porting checklist (future hosts)

See [`PORTING.md`](PORTING.md) and [`PROFILES.md`](PROFILES.md). Minimum bar:

- [ ] USB Host client runs continuous bulk without silent overrun storms  
- [ ] Profile identity filter (VID/PID/strings) fail-closed  
- [ ] Measured or formula-backed rate path  
- [ ] Soak log from **this** tree under [`lab/`](lab/)  
- [ ] CAP bits only for proven paths  
