# Hardware scope (intentional, not accidental)

This project **fails closed** on unsupported hardware. Narrow support is a
**design choice**, not an unfinished port of librtlsdr.

---

## Supported today (claimed)

| Piece | Status | Notes |
|---|---|---|
| **MCU** | ESP32-**P4** High-Speed USB Host | e.g. M5Stack Tab5, Waveshare P4 kits |
| **Dongle** | RTL-SDR **Blog V4** | USB `0bda:2838`, mfg/product `RTLSDRBlog` / `Blog V4` |
| **IQ** | Continuous CU8 multi-URB | CAP_STREAM |
| **Tooling** | ESP-IDF ≥ 5.3, `esp32p4` | CI builds smoke on 5.3.2 + 5.4.1 |

Evidence labels: [`../PROJECT_TRUTH.md`](../PROJECT_TRUTH.md).

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
