# AI Development Disclosure

**esp_rtl_sdr** is human-directed and AI-assisted. This document says plainly what
that means so nobody has to guess from the commit history.

This follows the same honesty model used on
[TheOrc](https://github.com/hardcoreerik/TheOrc)
([`docs/AI_DEVELOPMENT_DISCLOSURE.md`](https://github.com/hardcoreerik/TheOrc/blob/master/docs/AI_DEVELOPMENT_DISCLOSURE.md)).

---

## Who does what

| Role | Who |
|---|---|
| Creator, maintainer, product direction, release authority | [Erik / hardcoreerik](https://github.com/hardcoreerik) |
| Hardware owner / lab authority | Erik (P4 hosts, Blog V4, Heltec, Baofeng, Flipper — see [TESTING.md](TESTING.md)) |
| Architecture planning, implementation support, review | Claude Sonnet |
| Implementation support, adversarial review | OpenAI Codex |
| Implementation, docs, PROJECT_TRUTH audits, runtime critique | Grok Build |

Erik is not a professional software engineer. He directs AI agents that write,
review, and document this codebase — deciding what gets built, what ships, and
what gets rejected. **The agents write a large fraction of the code; Erik owns
the outcome.**

---

## What this project is (and is not)

| Claim | Truth |
|---|---|
| Stand-alone ESP-IDF component | **Yes** — this repo |
| Clean-room Blog V4 USB path | **Yes** — measured tables; **not** a copy of librtlsdr / rtl-sdr-blog source |
| Drop-in librtlsdr / `rtl-sdr.h` ABI | **No** — never a goal |
| Every RTL2832U dongle works | **No** — profile-based; Blog V4 first |
| All rates “P4 proven” | **No** — only 960k / 2.048M have **provenance** under OrcSDR; passport learns the rest **on your host** |
| Re-soaked from *this* tree on hardware | **Open** — tracked in [PROJECT_TRUTH.md](../PROJECT_TRUTH.md) |
| Formal third-party security audit | **No** |

If a README or release note sounds stronger than the table above, treat that as a
**bug** and report it.

---

## What this means for trust

Because the code is heavily AI-generated and the project has one human
maintainer rather than a large review team, esp_rtl_sdr holds itself to concrete
rituals:

1. **Claims require evidence labels.**  
   See [PROJECT_TRUTH.md](../PROJECT_TRUTH.md): Implemented / Provenance /
   Hardware-verified / Planned / Deferred / Formula. Docs must not market
   Planned as Done.

2. **Clean-room is non-negotiable for USB tables.**  
   Behavior may be *observed* (USB capture, public DS insight, R820T2 register
   PDFs). Source from librtlsdr / rtl-sdr-blog must **not** be pasted into this
   tree. See [CLEAN_ROOM.md](CLEAN_ROOM.md) and [SILICON.md](SILICON.md).

3. **Multiple AI reviewers when something ships past prototype.**  
   Prefer at least two of {Claude, Codex, Grok} on non-trivial driver changes
   before release tags. AI review is **not** a substitute for a formal security
   audit or independent crypto review (this driver has neither).

4. **Generated code is under the repo license** — same as human code.  
   See [LICENSING.md](../LICENSING.md).

5. **Doc/code mismatch is a bug.**  
   Report via GitHub issues, or email for security (see [SECURITY.md](../SECURITY.md)).

---

## Provenance honesty (OrcSDR → this repo)

Continuous Blog V4 IQ on ESP32-P4 was first measured under the **OrcSDR**
program (Tab5 + Waveshare). This repository is the **driver-only** extraction
and rename (`esp_rtl_sdr`), with further API work (desktop-shaped helpers,
continuous rates, need/health/passport) done here.

| Statement | Status |
|---|---|
| EP0 / profile tables came from clean-room measurement, not librtlsdr source | Claimed; clean-room rules apply |
| Same tables worked unmodified on a second P4 board (Waveshare) under OrcSDR | **Provenance** (OrcSDR), not yet re-logged from this tree’s example |
| `probe_rates` / continuous rates validated on P4 from *this* tree | **Implemented**; hardware log still open |
| Gain / bias-T | **Not implemented** — no CAP bit until measured |

---

## Where docs may drift

This project moves quickly under one maintainer + AI agents. Drift happens in
both directions:

- Design docs (`VISION.md`) may describe **direction** while code is partial —
  those must stay labeled Planned vs Implemented.
- `CHANGELOG` / version badges must match `ESP_RTL_SDR_VERSION_*` and
  `idf_component.yml`.
- OrcSDR READMEs may still describe an older in-tree component name
  (`rtl_sdr_v4_esp`); **this** repo is the stand-alone home going forward.

If you find a contradiction, open an issue titled `truth: …` so it is easy to
triage.

---

## Open source practice (TheOrc-aligned)

| Practice | How we apply it here |
|---|---|
| Ship in public | GitHub `hardcoreerik/esp-rtl-sdr`, tagged releases |
| Retract oversell | Prefer fixing docs/tags over defending a wrong claim |
| Dual license clarity | AGPL-3.0-only default + commercial option in LICENSING.md |
| Security contact | SECURITY.md — no public zero-days |
| No empty placeholder docs | DOCUMENTATION_STANDARD.md |
| Lab gear disclosed | TESTING.md (Heltec, Baofeng, Flipper, hosts) |
