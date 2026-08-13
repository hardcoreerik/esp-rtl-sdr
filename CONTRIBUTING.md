# Contributing to esp_rtl_sdr

Thanks for caring about a careful ESP32 RTL path. This project follows the same
**truth-first open source** habits as [TheOrc](https://github.com/hardcoreerik/TheOrc).

---

## Non-negotiables

1. **Clean-room USB behavior** — do not paste librtlsdr / rtl-sdr-blog / osmocom
   driver source into this tree. See [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md).
2. **Truth labels** — update [PROJECT_TRUTH.md](PROJECT_TRUTH.md) when behavior
   changes. Do not mark Hardware-verified without a run from **this** tree.
3. **Capability bits** — only set `CAP_*` for implemented paths.
4. **License** — contributions are AGPL-3.0-only (and commercial dual-license
   model as published). See [LICENSING.md](LICENSING.md).

---

## Good contributions

- USB captures + notes for gain, bias-T, R828D inputs (measured)
- Passport / soak logs from ESP32-P4 + Blog V4 — use [`docs/lab/SOAK_LOG_TEMPLATE.md`](docs/lab/SOAK_LOG_TEMPLATE.md)
- Bug fixes with reproduction steps
- Docs that **narrow** oversell rather than inflate claims
- Example and smoke improvements
- Issues that track real limits (prefix `truth:` if we oversold something)

### Good first issues

Look for labels **`good first issue`** and **`docs`**. Safe starters:

- Fill a soak log from your P4 + Blog V4 desk  
- Improve troubleshooting with a failure you hit  
- Host-test edge cases for pure policy (no hardware)  
- Editorial fixes that keep PROJECT_TRUTH accurate  

Harder tracks: Phase 3 USB capture, new profiles — read [`docs/CLEAN_ROOM.md`](docs/CLEAN_ROOM.md) first.

---

## PR checklist

- [ ] **Host unit tests pass** — `tests/scripts/run_host_tests.ps1` (or `.sh`)
- [ ] **Truth hygiene** — `tests/scripts/check_truth_hygiene.sh` (or rely on CI)
- [ ] Pure policy changes land in `src/esp_rtl_sdr_policy.cpp` when possible
- [ ] New pure behavior has a host assertion when practical
- [ ] Code builds as an ESP-IDF component (or you state you could not run IDF)
- [ ] Public API changes update `include/esp_rtl_sdr.h` + `docs/API_REFERENCE.md` (+ `docs/API.md` if contract changes)
- [ ] Version macros / CHANGELOG if releasing
- [ ] PROJECT_TRUTH evidence labels still accurate
- [ ] No GPL librtlsdr source paste
- [ ] No secrets, private serial dumps, or illegal TX instructions

---

## Development context

This repo is **human-directed, AI-assisted**, single maintainer today. See
[docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md).
External human review, hardware soak logs, and clean-room captures are especially valuable —
that is how we reduce key-person and validation risk without faking community.

---

## Security

See [SECURITY.md](SECURITY.md). Do not file public issues for unfixed security
flaws.
