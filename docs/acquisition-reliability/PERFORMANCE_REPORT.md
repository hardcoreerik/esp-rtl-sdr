# Performance and memory evidence

CU8 payload ownership and copies remain unchanged: USB buffer to CPU delivery
storage/ring, then caller copy for pull reads. Metadata adds no payload copy and
no per-block heap allocation. Optional integer quality adds work proportional
to the selected inspected pairs. Full scans should be measured on the target
before enabling them at high rates.

Initial baseline handle size was 2,512 bytes; acquisition adds 40 bytes even
when disabled. The 152-byte metadata implementation reserves 11,424 internal
bytes per opted-in handle, after USB buffers. Callback/BOTH retain six CPU IQ
slots; READ now skips those unused slots, saving 98,304 bytes for six 16-KiB
buffers. Pull-ring capacity and descriptor capacity remain separate budgets.

The first host benchmark optimized away the baseline memcpy and is rejected.
The corrected benchmark uses a volatile memcpy function pointer. Host timings
are not P4 cycle measurements and are not grounds for a target throughput claim.
Final measurements and source revision remain pending delivery verification.

In both initial single-device physical eight-second windows, 15,351,808 bytes
were observed (959,488 sample pairs/second), with zero window overruns/drops.
Later undrained transitions in the Tab5 smoke incurred known software loss;
these windows do not establish an overall lossless run.

Final host run (Linux Release, 20,000 blocks of 16,384 bytes): copy baseline
185.0 ns/block; copy plus metadata 247.4; metadata with stride64 quality 630.4;
full-scan quality 22,417.4. One run, not a statistical confidence interval.
Metadata is 152 bytes; host sidecar64 is 10,256 bytes. These numbers demonstrate
relative work only and are not microcontroller performance predictions.

Final 15-minute Waveshare run: continuous 2.4-MS/s receiver reported
4,320,247,808 observed bytes and 4,320,247,806 delivered at the final snapshot,
with one known lost pair and no USB errors. Peer retune/restart/release did not
change its epoch. This setup differs from the initial single-device Tab5
baseline; it cannot isolate the CPU cost of metadata. No target CPU utilization
or calibrated RF clock measurement was made.
