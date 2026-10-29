# Acquisition reliability architecture and approved design

Base: b283029dd0257ec207d5e94db3730077c40a23de. User approved the staged scope and delegated API design on 2026-10-29. COM39 is the designated Tab5; the user subsequently reauthorized flashing for regression tests. The Waveshare P4-Module-DEV-KIT with 7-inch Hosyond display is authorized on COM8; its built-in hub currently connects V4 and V3/R820T2-profile devices.

## Contract

Keep the existing C read API and IQ callback payload ABI unchanged. Add opt-in acquisition configuration, versioned per-buffer metadata, a new extended callback event and read_ex. Acquisition metadata is captured at USB completion, never synthesized from a later live metrics snapshot. Existing paths remain the default. Disabled handles grow by 40 bytes; runtime metadata storage is opt-in. READ skips unused callback buffers.

Each handle has an independent monotonically increasing 64-bit epoch. A successful start and any drained hardware-control interruption start a new epoch. Successful retunes use the newly applied configuration; failed partially applied controls begin an uncertain epoch with frequency/configuration explicitly unknown. Queued old samples retain their original metadata. Host timestamps describe client-task completion observation, not RF sampling, oscillator calibration or phase coherence.

The logical sample index counts complete CU8 pairs observed in completed transfers within the epoch. Known software discards advance the index and are reported in sample units; USB errors and interrupted capture set an unknown-loss flag without inventing a count. Epoch/index overflow fails closed or starts a new qualified epoch rather than silently implying continuity.

## Bounded delivery

Extended callback metadata travels inside the existing CPU-owned IQ slot; borrowed lifetime ends with the callback. USB buffers remain driver-owned and are promptly resubmitted. The opt-in pull path uses a bounded descriptor sidecar alongside the byte ring. A descriptor contains immutable metadata and a byte span. read_ex returns at most one descriptor span, including partial reads, so it never combines epochs. Legacy reads consume the same descriptors when acquisition is enabled. Descriptor exhaustion drops newest data and reports a known delivery gap rather than mislabeling bytes. No hot-path allocations or extra payload copies.

Reader serialization is separate from USB producer work. The producer never waits on a consumer mutex. Descriptor and byte publication use release/acquire ordering; consumer progress retires descriptors only after associated bytes have been copied. Per-channel loss accounting prevents a callback loss from masquerading as pull-ring loss.

## Timing and quality

Expose requested and programmed rates separately. Physical-clock calibration remains unknown unless an explicit validated application estimate is configured; no decoder dependency or USB-arrival-derived calibration. Optional bounded integer I/Q statistics report measured rail hits, I/Q means, power and peaks with inspected-sample counts. Sparse scans are explicitly estimates and preserve I/Q parity. Overload is an inference, not a calibrated RF measurement.

## Safety gates

Preserve all maintenance teardown/refcount gates. Do not claim empty-list enumeration cleanup or reliable repeated host installation. Establish physical baseline before metadata changes. Preserve the original COM39 application and partition layout; do not read credential partitions. Do not modify customer repositories, pins, versions, releases or master.
