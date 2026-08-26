# Closing external review gaps (2026-08)

Source: deep review of esp-rtl-sdr @ **v0.7.3** (pre–API_REFERENCE).  
This file maps each review finding → action. Honesty: some items need **lab hardware**
and cannot be truthfully closed with docs alone.

| # | Review finding | Status after this work | Evidence |
|---|---|---|---|
| 1 | Gain / bias incomplete | **0.7.5 tables + CAP on**; AUTO **0.7.8**; P4 re-soak / multimeter still open | [`PHASE3_CAPTURE_REPORT.md`](PHASE3_CAPTURE_REPORT.md), [`AGC_IF_CAPTURE.md`](AGC_IF_CAPTURE.md) |
| 2 | Narrow HW (P4 + V4 only) | **Documented as intentional scope** | [`SCOPE.md`](SCOPE.md), [#4](https://github.com/hardcoreerik/esp-rtl-sdr/issues/4) Deferred |
| 3 | Single maintainer + AI | **Disclosed** (not “fixed” — truth) | AI disclosure, CONTRIBUTING, [#5](https://github.com/hardcoreerik/esp-rtl-sdr/issues/5) |
| 4 | Empty issues/PRs | **Seeded tracking issues** | [#1](https://github.com/hardcoreerik/esp-rtl-sdr/issues/1)–[#5](https://github.com/hardcoreerik/esp-rtl-sdr/issues/5) |
| 5 | Limited soak evidence | **Procedure + template** — log still open | [`SOAK.md`](SOAK.md), [#2](https://github.com/hardcoreerik/esp-rtl-sdr/issues/2) |
| 6 | AGPL commercial friction | **Commercial process clarified** | [`LICENSING.md`](../LICENSING.md) |
| 7 | API / docs gaps | **Closed in-tree** | [`API_REFERENCE.md`](API_REFERENCE.md), KCONFIG, EXAMPLES, TROUBLESHOOTING, [#3](https://github.com/hardcoreerik/esp-rtl-sdr/issues/3) |
| 8 | No releases / tags | **Tags existed**; **GitHub Release** published | [v0.7.3 Release](https://github.com/hardcoreerik/esp-rtl-sdr/releases/tag/v0.7.3) |
| — | Magic numbers | **Documented** | [`RUNTIME_CONSTANTS.md`](RUNTIME_CONSTANTS.md) |
| — | DMA speculation | **N/A** — IDF USB host owns DMA; not a driver bug | — |
| — | S3 as P2 must-have | **Rejected as near-term priority** — Deferred with rationale | SCOPE + Roadmap |

## Still open (honest)

1. Phase 3 gain/bias **hardware CAP** (needs USB capture desk work).  
2. Stand-alone **soak log** filled under `docs/lab/` (needs P4 + dongle run).  
3. Additional dongle profiles / FS hosts (Deferred).  

When those land, update PROJECT_TRUTH in the **same** change set.
