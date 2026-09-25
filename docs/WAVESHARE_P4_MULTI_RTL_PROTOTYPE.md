# Waveshare ESP32-P4 multi-RTL prototype

Bring-up guide for concurrent RTL-SDR receivers on the
**Waveshare ESP32-P4-Module-DEV-KIT**.

Hardware results in this file are **not yet measured** from the
`esp-rtl-sdr-signal-anomaly` tree unless a later soak log is linked.

---

## Exact board

| Item | Value |
|---|---|
| Board | Waveshare ESP32-P4-Module-DEV-KIT (SKU 30560 family) |
| Module | ESP32-P4-Module (ESP32-P4NRW32 + ESP32-C6 + 16 MB flash) |
| PSRAM | 32 MB in-package |
| USB | USB OTG 2.0 HS to four Type-A connectors via mux + hub |
| Programming | Type-C UART (not the Type-A SDR ports) |

The driver is **not** board-specific. The same component ran on M5Stack Tab5
and on this Waveshare kit as a single-dongle host. This document is the
multi-receiver **prototype** procedure.

Chip revision: Waveshare examples distinguish P4 rev v3.1+, v3.0+, and
pre-v3 engineering samples. `examples/multi_rtlsdr_test` does **not** pin
Tab5 `REV_MIN_0`. Tab5 users must overlay
`examples/multi_rtlsdr_test/sdkconfig.defaults.tab5`.

---

## USB port topology

Official FAQ:

> Of the four Type-A ports, one is directly connected to the ESP32-P4, and
> the other three are connected to the CH334 HUB chip. Jumper caps switch
> the P4 between direct connection and the CH334.
>
> 1. Jumper on **HOST** → USB port 1 enabled.
> 2. Jumper on **DEVICE** → USB ports 2–4 enabled.

Schematic confirmation (Waveshare PDF):

- `U15` FSUSB42UMX analog mux between direct HS pair and hub upstream
- `U14` **CH334F** 4-port USB 2.0 HS hub
- Dual stacked Type-A jacks `J2` / `J8`

```text
                    HOST jumper          DEVICE jumper
                         |                     |
  ESP32-P4 USB HS ── FSUSB42 ──┬── USB-A port 1 (direct)
                               └── CH334F upstream
                                      ├── USB-A port 2
                                      ├── USB-A port 3
                                      └── USB-A port 4
```

**Four physical connectors ≠ four independent host ports.**  
Maximum simultaneous Type-A hosts: **3** (behind CH334) **or** **1** (direct).

---

## Jumper configuration

| Goal | Jumper | Ports |
|---|---|---|
| Gate 1 single dongle (simplest) | HOST | Port 1 only |
| Gate 2–5 two or three dongles | DEVICE | Ports 2, 3, 4 |
| Do not mix | — | Port 1 is dead when DEVICE is selected |

ESP-IDF: `CONFIG_USB_HOST_HUBS_SUPPORTED=y` is **required** for DEVICE
jumper (CH334). The multi-rtl example enables it. The Tab5 smoke example
does not; single-dongle on port 1 still works without a hub driver.

---

## Power

| Rail | Notes |
|---|---|
| Board 5 V | USB-C and/or 5 V header. Use a supply rated for the P4 + hub + dongles |
| Hub VBUS | CH334 downstream; board current-limit switch (DIO7003-class) on 5 V |
| RTL-SDR Blog V4 | Typical ~180–300 mA without bias-T (vendor / community; **not measured here**) |
| Three dongles | Roughly 0.6–1.0 A extra on 5 V plus hub/P4 — **estimate, not a measurement** |

**Keep bias tees OFF** during initial multi-dongle tests.

If rails sag, LEDs blink, or USB re-enumerates:

1. Stop.
2. Record it as an electrical event, not a driver bug.
3. Retest with a powered hub or a higher-current 5 V supply if available.

No voltage measurements have been taken from this tree.

---

## ESP-IDF configuration

| Symbol | Multi-rtl example | Why |
|---|---|---|
| `CONFIG_USB_HOST_HUBS_SUPPORTED` | y | CH334 downstream devices |
| `CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE` | 1024 | RTL EP0 |
| Target | esp32p4 | HS host |

IDF 5.5.4 is the CI pin. Hub driver limitations: no TT (FS/LS behind HS hub),
incomplete overcurrent handling.

---

## Physical dongle connection

1. Power the board from Type-C (or 5 V header). Do not back-power from a
   Type-A port.
2. Set jumper for the test gate.
3. Insert RTL-SDR.com V4 (or other accepted profile) dongles.
4. Serial console is the Type-C UART, not a Type-A port.

---

## Tests

Firmware: `examples/multi_rtlsdr_test/`

### Gate 1 — single dongle regression

HOST jumper, one dongle in port 1 (or DEVICE + one stick in port 2).

```
rtl list
rtl info 0
rtl freq 0 99100000
rtl stream 0 start
rtl stats
```

Expect: tuner detection, stable stream, existing 960k/2.4M behaviour.

### Gate 2 — two-device enumeration

DEVICE jumper, two dongles in ports 2 and 3.

```
rtl list
rtl info 0
rtl info 1
```

Expect: two USB addresses, two `[RTL0]` / `[RTL1]` log lines, independent
identity.

### Gate 3 — two-device concurrent streaming

```
rtl freq 0 99100000
rtl freq 1 101100000
rtl stream all start
rtl watch 5000
```

Target: 2.4 MS/s each, ≥10 minutes (30 minutes once stable). Record stats.

### Gate 4 — three-device enumeration

Third dongle in port 4. `rtl list` shows three.

### Gate 5 — three-device concurrent streaming

```
rtl freq 0 99100000
rtl freq 1 101100000
rtl freq 2 103100000
rtl stream all start
```

If 3 × 2.4 MS/s is not sustainable, step down rate and record the boundary.
Do not hide a limitation.

### Gate 6 — independent live retune

While streaming:

```
rtl freq 1 103100000
rtl gain 1 240
```

Expect RTL0 and RTL2 counters to keep incrementing.

---

## Known limitations

- Not measured on this branch yet.
- No sample-sync / phase coherence.
- IDF hub: no TT, weak overcurrent recovery.
- `usb_timeouts` counter will stay 0 until IDF grows transfer timeouts.
- Tab5 USB-A power expander is **not** driven by this example.
- Four Type-A jacks cannot host four concurrent dongles on this board.
