# Hardware soak — procedure (this repository)

**Goal:** Produce **Hardware-verified** evidence from a **build of this tree**
on named ESP32-P4 + Blog V4 hardware.

Until a filled log is committed under [`lab/`](lab/), stand-alone re-soak remains
**open** in [`PROJECT_TRUTH.md`](../PROJECT_TRUTH.md). OrcSDR results are
**Provenance** only.

---

## Why this matters

Reviewers correctly note: host CI + IDF compile ≠ continuous USB/RF truth.
Passport API + smoke are **Implemented**; a multi-hour soak log is the missing
artifact for production confidence.

---

## Minimum soak (acceptance bar)

| Item | Requirement |
|---|---|
| Host | Named P4 board (e.g. Tab5 / Waveshare P4) |
| Dongle | Blog V4 serial (redact middle if public) |
| Tree | `git describe` / commit SHA of **this** repo |
| IDF | Version string |
| Duration | Prefer **≥ 1 h** continuous; **24 h** for “long soak” claim |
| Rates | At least one of 960k and 2048k; optional full `probe_rates` |
| Metrics | Log overruns, consumer_drops, effective_sps, health overall |
| Result | Pass if no FAULT, no unbounded overrun growth, efficiency ≥ lab threshold |

---

## Procedure

1. Checkout this repo at a known tag/commit.  
2. Build `examples/p4_serial_smoke` (or your app linking the component).  
3. Flash, plug Blog V4 into **USB Host**.  
4. Optional: `probe_rates` with dwell ≥ 1000 ms, `recommended_only=true`.  
5. Stream at target rate/LO for the soak duration.  
6. Periodically log `get_metrics` + `get_health` (e.g. every 30–60 s).  
7. Copy serial log + fill [`lab/SOAK_LOG_TEMPLATE.md`](lab/SOAK_LOG_TEMPLATE.md).  
8. PR: add `lab/SOAK_<board>_<date>.md` + update PROJECT_TRUTH labels.  

Legal RF only — see [`LAB_HOBBYIST.md`](LAB_HOBBYIST.md).

---

## Template location

- Blank form: [`lab/SOAK_LOG_TEMPLATE.md`](lab/SOAK_LOG_TEMPLATE.md)  
- Filled logs: `lab/SOAK_*.md` (none yet → soak still open)

---

## CI boundary

| Gate | What it proves |
|---|---|
| Host policy tests | Pure rate/config math |
| `idf-p4-build` | Smoke compiles for esp32p4 |
| Lab soak log | Continuous USB/host sustainability **on hardware** |

Do not mark Hardware-verified without a log from this tree.
