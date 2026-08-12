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
- Passport / soak logs from ESP32-P4 + Blog V4
- Bug fixes with reproduction steps
- Docs that **narrow** oversell rather than inflate claims
- Example and smoke improvements

---

## PR checklist

- [ ] **Host unit tests pass** — `tests/scripts/run_host_tests.ps1` (or `.sh`)
- [ ] Pure policy changes land in `src/esp_rtl_sdr_policy.cpp` when possible
- [ ] Code builds as an ESP-IDF component (or you state you could not run IDF)
- [ ] Public API changes update `include/esp_rtl_sdr.h` + `docs/API.md`
- [ ] Version macros / CHANGELOG if releasing
- [ ] PROJECT_TRUTH evidence labels still accurate
- [ ] No GPL librtlsdr source paste
- [ ] No secrets, private serial dumps, or illegal TX instructions

---

## Development context

This repo is **human-directed, AI-assisted**. See
[docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md).
External human review and hardware logs are especially valuable.

---

## Security

See [SECURITY.md](SECURITY.md). Do not file public issues for unfixed security
flaws.
