# Dongle regression: nooelec-hotplug-B2-speakerfix

- Dongle: **nooelec_smart_v5_r820t2**, driver 0.9.2, pin 3787e1015d136f28f68be8c1411e1fac2b4e00d6
- OrcSDR: claude/tab5-abc-C2@5f788a1, level Quick, antenna ADS-B dipole east window, 2026-10-01T21:40:15

| Stage | Result | Detail |
|---|---|---|
| preflight | PASS | nooelec_smart_v5_r820t2, driver 0.9.2 (pin 3787e1015d), state STREAMING, gain=True |
| stress | PASS | 18 band switches over 2 rounds; problems: none |
| hotplug | PASS | asked 5: disconnects=6 probes=6 restarts=6 start failures=0 reboots=0 leaked URBs=0 |
