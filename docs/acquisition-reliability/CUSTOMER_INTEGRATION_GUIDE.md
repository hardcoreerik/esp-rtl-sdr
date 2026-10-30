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

Minimal setup after installing a READ/BOTH handle:

```c
esp_rtl_sdr_acquisition_config_t acquisition;
esp_rtl_sdr_acquisition_config_default(&acquisition);
acquisition.enabled = true;
esp_err_t err = esp_rtl_sdr_configure_acquisition(handle, &acquisition);
/* Check err; on success, start capture using the existing start API. */
```

For each read, initialize `esp_rtl_sdr_sample_meta_t` with its default function,
pass a caller-owned byte buffer and byte-count output to `read_ex`, and inspect
the record only on ESP_OK. Timeouts are not evidence of a measured sample gap;
gap accounting comes from subsequent metadata and device/error state.

Use existing health/passport/window statistics together with metadata for an
application-specific assessment:

| Observation | Appropriate action or conclusion |
|---|---|
| New receiver/epoch, known or unknown gap | Reset continuity-dependent decoder state |
| Known software loss or rising ring/metadata pressure | Drain faster, reduce rate or adjust storage within the measured budget |
| USB errors/disconnect | Mark physical loss unknown; follow normal stop/cleanup recovery |
| CU8 rails or strongly displaced means | Flag a signal-quality concern; test RF/gain setup before assigning a cause |
| Nominal rate only, no external reference | Physical clock accuracy remains unknown |
| Zero errors in a bounded run | Evidence for that setup/window, not a universal reliability guarantee |

No additional reliability-score API is introduced: a single score would hide
these different evidence boundaries and duplicate existing counters.
