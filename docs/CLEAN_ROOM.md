# Clean-room rules — esp_rtl_sdr

## Policy

esp_rtl_sdr is implemented from:

1. **Independent USB observation** of physical dongles (captures, hashes, notes).
2. **Public** USB, ESP-IDF, and chip documentation.
3. **Measured** ESP32-P4 host results.

It is **not** implemented by reading, translating, or copying:

- librtlsdr
- rtl-sdr-blog
- osmocom rtl-sdr trees
- peer ESP projects that embed those sources

Architecture ideas (dual-core pin, ring buffers, rtl_tcp as an *app*) may be
learned from peers. **Register sequences and I2C tables may not.**

## Blog V4 profile provenance

The Blog V4 init/rate/tune tables in `private/transfers_blog_v4.hpp` originate
from black-box observation of an official RTL-SDR Blog V4 (`0bda:2838`) as
documented historically in OrcSDR’s clean-room materials. This repo copies those
tables as the first profile; it does not re-license third-party driver code.

## Adding a feature (gain, bias, new rate, new tuner)

1. Capture USB on a PC or instrumented host with the **same** physical dongle class.
2. Record procedure, identity (VID/PID/serial/strings), and hashes.
3. Implement only from that evidence.
4. Validate on ESP32-P4 (or claimed host).
5. Update `Project_truth.md` and `docs/CAPABILITY_MATRIX.md`.
6. Enable capability bits only when the path works.

## Expected STALLs

Some dongles STALL vendor control transfers when probing absent I2C addresses.
Only STALLs **independently observed** for a profile may be treated as
non-fatal. Do not import “known STALL lists” from librtlsdr comments.

## Review checklist

- [ ] No new `#include` of foreign rtl driver sources
- [ ] No pasted register arrays from GitHub librtlsdr
- [ ] New rates/gains have evidence notes
- [ ] Unknown devices fail closed
