# Decoder-grade acquisition implementation plan

> For agentic workers: use superpowers:executing-plans inline; one focused branch review at the end.

Goal: optional byte-associated integrity metadata and truthful timing/quality diagnostics for independent applications.
Architecture: per-handle epoch/timeline plus bounded metadata sidecar; existing APIs/default paths preserved.
Tech stack: C++17, C API, ESP-IDF 5.5.4 and 6.1, CMake/CTest on Windows and Linux.
Spec: ARCHITECTURE.md and the user's approved Phase 2 request.

## Global constraints

No master merge, version/tag/release, customer repo changes, copied tuner code, network/decoder dependency, per-sample allocation or hardware claims without logs. COM39 is under a no-flash hold. Waveshare COM8 is authorized. Retain original firmware recovery artifacts without extracting credentials.

## Review focus

Old queued bytes after retune; partial read boundaries; independent receiver loss counters; metadata exhaustion without payload overwrite; failed controls and asynchronous coalescing.

## Task 1: Baseline and safety

- [x] Verify branch/base, existing tests/builds and physical COM39 identity; preserve application/partition recovery artifacts.
- [x] Run existing smoke on COM39 and investigate failures before new acquisition code.
- [x] Record unresolved enumeration/SDK gates honestly.

## Task 2: Integrity model and APIs

Files: include/esp_rtl_sdr.h, private/rtl_acquisition.hpp, tests/host/test_acquisition.cpp, tests/host/CMakeLists.txt, API_DESIGN.md.
Interfaces: versioned acquisition config and metadata; configure_acquisition(handle, config); read_ex(handle, bytes, capacity, timeout, out_bytes, metadata); new borrowed extended IQ event.
- [x] Write failing host tests for epochs, known/unknown loss, independent receivers, wrap, bounded descriptors and partial reads.
- [x] Implement and verify pure acquisition/descriptor logic.
- [x] Integrate completion snapshots, control/start boundaries, callback slots and the opt-in pull sidecar; verify actual runtime with fault-injected platform calls.
- [x] Verify C and existing C++ API consumers; commit with docs and measured memory cost.

## Task 3: Timing, quality and assessment

Files: private/rtl_acquisition.hpp, src/esp_rtl_sdr.cpp, public API, focused host tests, PERFORMANCE_REPORT.md.
- [x] Test requested/programmed/unknown physical rates and integer I/Q quality against generated buffers.
- [x] Implement optional measurements and an evidence-scoped reliability assessment without duplicating existing health/passport counters.
- [x] Benchmark identical synthetic workloads before/after; run builds and physical metadata smoke; commit.

## Task 4: Multi-device, portability and delivery

Files: customer-neutral smoke/harness, HARDWARE_PORTABILITY.md, TEST_PLAN.md, CUSTOMER_INTEGRATION_GUIDE.md, IMPLEMENTATION_REPORT.md, PROJECT_TRUTH.md.
- [ ] Test independently identified handles, different rates, loss/retune/restart/disconnect isolation and resource exhaustion.
- [x] Verify official P4/S31/S2/S3 capabilities and installed target/toolchain support; compile only justified targets and label evidence separately.
- [x] Execute available physical tests/soaks with per-receiver records; retain NOT RUN for unavailable setups.
- [ ] Complete documentation and final focused review; commit coherent changes and push only the feature branch.

Task 4 qualification: retune/restart/release isolation and injected resource failures passed; physical disconnect/replug remains NOT RUN. Independent review was attempted but could not run because of account usage limits. Final long run passed; implementation and evidence are delivered on the feature branch. Review remains open; no merge or release.
