# Integrating acquisition metadata

The driver remains an independent ESP-IDF component. No decoder, networking,
display or customer repository is required. Existing applications can continue
using their current read/callback interfaces with acquisition disabled.

For a new consumer:

1. Install a handle using the existing board/application USB power setup.
2. Choose READ, CALLBACK or BOTH. Reserve an appropriate pull ring for READ.
3. Enable acquisition before first start; keep quality disabled initially.
4. Start at a supported rate, then consume extended spans promptly.
5. Reset decoder continuity when receiver/epoch changes or gap flags appear.
6. Store metadata with its exact bytes. Never label queued data using a later
   live metrics snapshot. Copy callback data before returning if retaining it.
7. Stop/join application readers before uninstall. Respect cleanup-only handles
   after failed teardown; a failed cleanup is not permission to free resources.

Use `examples/acquisition_probe` for a display-free serial harness. A blank
display is expected: this program does not initialize display or touch hardware.
Board USB routing, power switches and hub jumpers remain board responsibilities.
The probe is test firmware, not an OrcSDR user interface.

See [API design](API_DESIGN.md) for loss and timing semantics. Calibrate sample
clock externally if a decoder needs physical-rate accuracy; USB arrival timing
is not a calibration reference. RF acceptance must record antenna/band suitability.
