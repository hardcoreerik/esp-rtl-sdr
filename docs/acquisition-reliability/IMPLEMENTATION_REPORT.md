# Implementation status — 2026-10-29

Feature branch: `feature/decoder-grade-acquisition-20261029`, based on maintenance
commit `b283029dd0257ec207d5e94db3730077c40a23de`. No master merge or release.

Implemented: optional extended C API, completion-time immutable metadata,
per-receiver epochs and observed sample indices, per-channel known delivery
loss, unknown interruption markers, bounded pull descriptors, partial reads,
optional integer quality and explicit sample-clock uncertainty. READ skips
unused callback buffers. Failed recovery drain retains outstanding resources.
Opted-in control boundaries also replay the existing captured FIFO-reset pair;
conducted RF settling/timing is not established by that digital fence.

Nooelec SMArt v5 is **Implemented**, not experimental or provisional. This is
supported by first-party control captures, host regressions and standalone Tab5
streaming/control checks. RF calibration and per-band qualification remain
separate, as for other implemented profiles. The Waveshare dual-device run
used V4 and V3/R820T2, not Nooelec.

Current verification and pending qualification are listed in [test plan](TEST_PLAN.md).
Final review, final hardware refinement/soak evidence, final SDK matrix and
feature commit/push are still in progress. The requested independent reviewer
could not run because its account usage limit was reached; no clean independent
review is claimed.

COM8 currently runs a serial-only test program, so the Hosyond display is blank.
Its original factory application and partition table remain backed up locally.
COM39 remains under the user's no-flash restriction. Neither board's original
application has been restored. Recovery backups and raw hardware logs are
local ignored artifacts, not committed release deliverables.

Unresolved maintenance gates remain: conservative cleanup when enumeration-idle
cannot be proved, late attachment between ALL_FREE and uninstall, and reliable
host reinstall after those failures. Cleanup errors must not be treated as a
successful release of ownership.
