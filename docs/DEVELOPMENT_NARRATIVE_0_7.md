# Development narrative — 0.7.x (through v0.7.3)

Verbose project commentary for readers of the git history and for future
maintainers. Authoritative *truth labels* remain in [PROJECT_TRUTH.md](../PROJECT_TRUTH.md).
Release deltas remain in [CHANGELOG.md](../CHANGELOG.md).

**Snapshot:** tip of the 0.7.3 line (`v0.7.3` / async retune). Still **0.x** —
architecture is strong; production bar is “boring under load + lab evidence,”
not “feature-complete vs desktop librtlsdr.”

---

## Where the project is

**esp_rtl_sdr** is a stand-alone ESP-IDF USB Host driver for RTL2832U-class
sticks (Blog V4 first). It is intentionally **not** a librtlsdr port: clean-room
EP0 tables, fail-closed lifecycle, capability flags, and honest “truth” docs.

This repo was split so the driver can be a reusable component instead of an
OrcSDR-only hardware path. Board BSP (display, VBUS, network, audio) stays
outside the component.

---

## What the 0.7.x slices actually did

### 1. Open-source / truth posture (TheOrc-style)

Human-directed, AI-assisted disclosure; `PROJECT_TRUTH` as authority;
`DOCUMENTATION_STANDARD` (planned ≠ done); security contact; contributing
rules. Goal: **oversell is a bug.**

Related: `docs/AI_DEVELOPMENT_DISCLOSURE.md`, `CONTRIBUTING.md`, `SECURITY.md`.

### 2. Continuous rates + “nervous system” APIs

- **Rates:** hardware windows, quantize to exact SPS. Low floor is **225001 Hz**,
  not 225000 (28-bit ratio mask zeroes at 225 kHz; matches desktop rejection of
  `rate <= 225000`).
- **Intent / health / passport:** `apply_need`, `get_health` + live `EVT_HEALTH`,
  on-device rate passport probe.
- **Phase 3 gain/bias:** public API **stubs only** (`ERR_UNSUPPORTED`, CAP bits
  off) until USB capture. See `docs/GAIN_BIAS_CAPTURE.md`.

### 3. Automated testing

- Host policy unit tests (no IDF) under `tests/host`.
- Truth/version hygiene script.
- CI: Ubuntu + Windows host tests; **ESP-IDF P4 compile** of
  `examples/p4_serial_smoke` on IDF **v5.3.2** and **v5.4.1**.

**Green CI means** policy + **compile**. It does **not** mean USB/RF soak.

### 4. Runtime hardening (0.7.2) — external review response

External review argued the biggest risks were concurrency/lifecycle, not more
RF reverse engineering. 0.7.2 paused Phase 3 hardware for hardening:

| Issue | Fix |
|---|---|
| Concurrent `start()` race | `STARTING` state → second start `BUSY` |
| Uninstall UAF / 50 ms hope | Worker tasks notify join waiter |
| Callback under lock | Atomic `in_callback_depth`; `select_device*` emits after unlock |
| Partial IQ-ring alloc | Transactional allocate + destroy on failure |
| `struct_size` exact match | Accept min..sizeof (append-only ABI) |
| Kconfig unused | Wired into `config_default` under IDF |
| Byte-loop pull ring | Block `memcpy` |
| Component honesty | `targets: [esp32p4]` in `idf_component.yml` |

Detail: `docs/HARDENING_0_7_2.md`.

### 5. True async retune (0.7.3)

Review choice: either strict `ERR_REENTRANT` or **real** async — not “return OK
and maybe never apply.”

| Who calls `retune_hz` / streaming `set_center_freq` | Behavior |
|---|---|
| **App task** | Queue LO → apply now (drain bulks → EP0 → resubmit) → `EVT_RETUNED` |
| **Event callback** (e.g. `EVT_IQ_BLOCK`) | Queue LO → **`ESP_OK` immediately** → delivery task applies later → **`EVT_RETUNED`** |

Still never EP0 while bulk is outstanding. Newer LO while apply is in flight is
**coalesced**. App pattern: retune from IQ callback is legal; wait for
`EVT_RETUNED` for “LO is live.”

---

## Lab honesty

Hobbyist desk is enough for **USB capture + multimeter + relative TinySA ΔdB**,
not for cal tables. Inventory and TinySA how-to: `docs/LAB_HOBBYIST.md`.
Bias USB first, then gain steps; TinySA does not replace USBPcap.

---

## What’s left (priority order at time of writing)

1. ~~ESP-IDF P4 compile CI~~
2. ~~Async retune from callback~~
3. **Delivery modes (CALLBACK / READ / BOTH) + lazy pull ring** — cut wasted
   copies when apps only use events
4. **Lab soak** from this tree (960k / 2.048M, start/stop/retune)
5. **Phase 3 gain/bias** from real captures

---

## Bottom line

The repo has the shape of an **embedded RTL host framework** (capabilities,
health, passport, fail-closed API), with CI that **builds the P4 smoke app**.
Remaining risk is less “can we reverse RF?” and more “does the data path and lab
evidence hold under real use?”

Next software step: delivery-mode / copy reduction.  
Next hardware step: bias USB capture or soak from this tree.
