# Acquisition API

Include `esp_rtl_sdr_acquisition.h`. Initialize configuration with
`esp_rtl_sdr_acquisition_config_default`, set `enabled=true`, and call
`esp_rtl_sdr_configure_acquisition` in IDLE before the first start. READ/BOTH
consumers initialize metadata with `esp_rtl_sdr_sample_meta_default` and call
`esp_rtl_sdr_read_ex`. Check the return value and returned byte count; a successful
read may be shorter than requested and never crosses a descriptor boundary.

CALLBACK/BOTH users receive the existing IQ event and the additional
`ESP_RTL_SDR_EVT_IQ_BLOCK_EX`. The extended payload and its data are borrowed until
the callback returns. Consume one event family to avoid processing data twice.
Do not retain borrowed pointers or uninstall from a callback.

`receiver_id` identifies a boot-local handle instance. `epoch` changes after
start or a drained control interruption. `sample_index` counts observed CU8 pairs
within that epoch, including subsequently discarded pairs; it cannot count
samples lost before USB observation. `known_lost_samples` describes omissions
for that delivery channel. `unknown_gap_count` counts interruption markers,
not missing samples. Old queued bytes retain their old frequency and epoch.

Partial reads change the returned sample index/count. Block timestamp, quality,
block index and block count continue to describe the original observed block.
An odd transfer makes alignment uncertain and faults capture until restart.
Exhaustion of a 64-bit timeline fails closed.

Host timestamps record client-task completion observation. Requested rate,
integer programmed rate, nominal ratio-derived milli-samples/second, RF tuning
ppm and optional application-provided sample-clock estimates are distinct.
No physical calibration, RF timestamp, synchronization or phase coherence is
inferred. Zero uncertainty means unquantified, not perfect accuracy.

Quality is disabled by default. Stride 1 scans all pairs; larger strides are
sparse estimates. Statistics describe unsigned CU8 codes around 127.5, not
calibrated signal power. Rail hits alone cannot identify an overloaded RF stage.

Configuration/metadata require the documented exact struct size. The header
documents errors and ownership. Serialize handle ownership with uninstall;
reader serialization does not make concurrent destruction safe.
