# Device profiles — esp_rtl_sdr

## Concept

A **profile** is a measured package of identity rules + USB control sequences +
tuner policy for one dongle class. The core host client stays shared.

## Active profiles

### `blog_v4` (RTL-SDR Blog V4 / R828D)

| Field | Value |
|---|---|
| Status | **Implemented** (tables in-tree) |
| USB | VID `0x0BDA` PID `0x2838` (plus product/mfg checks in code) |
| Tuner | R828D (as identified on official Blog V4) |
| Tables | `private/transfers_blog_v4.hpp` |
| Rates | 960k, 1024k, 2048k allowlist |
| Provenance | Clean-room captures; ESP32-P4 measured under OrcSDR Tab5 + Waveshare |

## Planned profiles

| Profile | Typical hardware | Status |
|---|---|---|
| `r820t2` | Common “RTL-SDR” R820T2 sticks | Planned — needs own captures |
| OEM ID variants | Same silicon, different PID | Planned — allowlist only when tested |

## Profile checklist (new dongle class)

1. Record USB device descriptor strings and VID/PID.
2. Capture full init + one tune + one rate change + cleanup.
3. Note expected STALLs (if any) with indices.
4. Implement profile module; wire into accept + start paths.
5. Soak on ESP32-P4 HS host.
6. Document in this file + `PROJECT_TRUTH.md`.

## Fail closed

If no profile accepts the device:

- Do not claim interface half-way.
- Return a clear error (`ERR_NOT_V4` / future `ERR_UNSUPPORTED_DEVICE`).
- Leave host stack consistent for other clients if shared.
