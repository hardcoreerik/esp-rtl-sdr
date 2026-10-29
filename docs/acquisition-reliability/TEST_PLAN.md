# Focused regression and hardware plan

Host commands, last executed 2026-10-29:

```
cmake --build tests/host/build --config Debug
ctest --test-dir tests/host/build -C Debug --output-on-failure
cmake --build build/host-linux -j4
ctest --test-dir build/host-linux --output-on-failure
python3 tests/scripts/test_nooelec_runtime_trace.py
```

Results: Windows and Linux 19/19 suites passed; Nooelec trace 7,990 checks passed.
Linux builds use warnings as errors. Tests include pure metadata arithmetic,
actual extracted producer/read functions with a two-thread 20,000-block test,
partial reads, queued old epochs, descriptor/ring pressure, channel isolation,
C linkage, allocation failures, recovery drain failures and captured FIFO reset
error propagation. Mocks do not prove the real USB scheduler or physical RF.

Physical records are retained under ignored `build/phase2-evidence`:

| Setup | Result / limit |
|---|---|
| Tab5 COM39, Nooelec v5, maintenance baseline | 45 checks passed |
| Tab5 COM39, initial acquisition prototype | 48 checks passed; later refinements untested there |
| Waveshare COM8, V4 alone | 46 passed, 1 skipped |
| Waveshare COM8, V4 + V3/R820T2 | Initial 60-second shared-host run passed; independent rates, retune/restart/release |

COM39 is under an explicit no-flash hold. COM8 is authorized. Application and
partition backups precede application-only writes; no credential partitions
were extracted. Final refinement runs must be recorded separately from prototypes.

Remaining qualification: physical disconnect/replug, late attachment during
final teardown, externally referenced clock accuracy, conducted RF transition
boundaries, calibrated gain/passbands, physical PSRAM and S31 hardware. No
blanket lossless, repeated-host-reentry or phase-coherence claim is permitted.

Additional targeted sanitizer command:
```
cmake -S tests/host -B build/host-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build/host-asan -j4
ctest --test-dir build/host-asan -R "acquisition|recovery" --output-on-failure
```
Result: 5/5 selected suites passed with AddressSanitizer/UndefinedBehaviorSanitizer.
Truth hygiene: `bash tests/scripts/check_truth_hygiene.sh` passed for 0.9.3.

Final COM8 refinement runs: 60-second PASS and 900,003-ms PASS. Final long run
V4 receiver0: 1,324,711,936 delivered bytes / 80,854 spans, zero known loss;
V3/R820T2 receiver1: 4,320,247,806 bytes / 263,687 spans, one known lost pair.
Both reported zero metadata invariant and USB errors. Retune, rate restart,
peer release and final cleanup returned ESP_OK. This crosses 4 GiB on receiver1;
it does not claim an entirely lossless stream. Exact hashes/configuration and
UTC capture bounds are in IMPLEMENTATION_REPORT.md.
