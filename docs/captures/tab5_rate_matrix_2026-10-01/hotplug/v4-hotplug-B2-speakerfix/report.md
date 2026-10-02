# Dongle regression: v4-hotplug-B2-speakerfix

- Dongle: **blog_v4_r828d**, driver 0.9.2, pin b91c4f0c6784dd5ea9181d561e31af859b37b52e
- OrcSDR: claude/tab5-abc-B2@eba15b2, level Quick, antenna ADS-B dipole east window, 2026-10-01T20:40:05

| Stage | Result | Detail |
|---|---|---|
| preflight | PASS | blog_v4_r828d, driver 0.9.2 (pin b91c4f0c67), state STREAMING, gain=True |
| stress | PASS | 18 band switches over 2 rounds; problems: none |
| gain | PASS | IQ RMS at 0/25.4/49.6 dB: 3.2 / 78.8 / 89.7 |
| hotplug | PASS | asked 5: disconnects=6 probes=6 restarts=6 start failures=0 reboots=0 leaked URBs=0 |
