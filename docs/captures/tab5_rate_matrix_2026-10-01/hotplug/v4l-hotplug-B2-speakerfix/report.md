# Dongle regression: v4l-hotplug-B2-speakerfix

- Dongle: **blog_v4l_r828s**, driver 0.9.2, pin 3787e1015d136f28f68be8c1411e1fac2b4e00d6
- OrcSDR: claude/tab5-abc-C2@5f788a1, level Quick, antenna ADS-B dipole east window, 2026-10-01T21:25:51

| Stage | Result | Detail |
|---|---|---|
| preflight | PASS | blog_v4l_r828s, driver 0.9.2 (pin 3787e1015d), state STREAMING, gain=True |
| stress | PASS | 18 band switches over 2 rounds; problems: none |
| gain | FAIL | IQ RMS at 0/25.4/49.6 dB: 1.9 |
| hotplug | PASS | asked 5: disconnects=6 probes=6 restarts=6 start failures=0 reboots=0 leaked URBs=0 |
