# esp_rtl_sdr — External driver findings, 2026-10-29

## Executive summary

Przemek Godek — https://github.com/pgodek — reported an attached-dongle uninstall
abort, memory pressure without PSRAM, a Tab5-only smoke check on another board,
and strong-signal clipping with tuner AUTO. His reports and workloads informed
this investigation. The changes here were implemented independently; no external
patch, network server, tuner table or register sequence was copied or adapted.

Source review confirms missing device-free cleanup, unchecked joins and cleanup
errors, optional buffers allocated ahead of USB transfers, and unconditional
Tab5 power access. Host fault injection checks the new ordering and error paths.
There was **no physical reproduction or hardware test** in this session.

**Release readiness: blocked.** Incomplete enumeration is not solved by a public
IDF API that proves the enumeration pipe is idle. When `device_free_all()` returns
success immediately for an empty list, this branch retains the host and returns
`ESP_ERR_INVALID_STATE`. This deliberately includes the no-dongle case. A reboot
may be required; the smoke uninstall/overall rows FAIL in that case. A late
attachment between ALL_FREE and final uninstall also remains a hardware/SDK
investigation item. Do not describe this branch as eliminating every uninstall
abort or as restoring reliable host re-entry.

ESP-IDF 6.1 is a useful additional development target: this branch adds its
managed USB dependency and an explicit C++17 selection while retaining 5.5.4.
Successful compilation does not settle USB lifecycle behavior on either SDK.

## Source and environment

| Item | Revision / evidence |
|---|---|
| Official baseline | `73ea1fbd3fbd404842bdc07ccc284dee276b9fc3`, current `origin/master` at scout |
| Original checkout | `F:\Ai\ESP-RTL-SDR\driver`, clean `ESP32-Fullspeed-USB-testing`, three commits behind master; preserved |
| Maintenance branch | `fix/external-driver-findings-20261029` in sibling `external-driver-findings` worktree |
| External uninstall branch | `ed66b62a149d5a982d7a43c9235d421f34f8437e`; behavioral patch `dc0cf9d5807f8dbf38aeb45da6843475923f4c87` |
| External server branch | `6037d9a8787cfa12519a28012584324f186d9011` |
| ESP-IDF 5.5.4 | `735507283d5b2f9fb363a1901172dbd9e847945d`, local `v5.5.4-dirty`; existing two-line SDIO source modification was not changed |
| ESP-IDF 6.1 | tag `v6.1`, `fff9895c82d744c7237be8847347bdd1b07c6643`; managed `espressif/usb` 1.6.0 |
| ESP-IDF 5.5.6 | Not installed; NOT RUN |
| Windows host compiler | MSVC 19.50.35725.0; Visual Studio 18 2026 generator |
| Linux host compiler | WSL Ubuntu GCC 13.3.0, `-Wall -Wextra -Werror` |
| Hardware | No dedicated board/dongle identified or authorized for flashing; no serial port opened |
| Serial inventory only | COM5, COM6, COM8, COM17, COM22, COM23, COM24, COM28, COM29, COM30, COM39; identities unverified |

Read the actual project truth, contribution, API, scope, soak, versioning,
documentation/testing instructions, current release notes, CI and host setup.
No additional repository or parent AGENTS.md was found; the user's supplied
scout/cost rules apply. The app worktree API rejected the nested checkout path,
so Git created the sibling worktree. No OrcSDR/OrcNode checkout or pin changed.

External review covered the uninstall diff and its reported successful and
zero-candidate failed runs; the server README, tests README, test inventory and
public-API control/start/stop call sites. The 4.80 MB/s/120-second figures and
55% clipping belong to Przemek's one-board, one-V4, IDF 5.5.6 report. They are
not measurements of this branch. His stalled-consumer, retune, rate-change and
gain-transition workloads remain useful physical acceptance procedures; the
server implementation and its protocol tests were not imported or run.

## A. USB lifecycle

Affected source: `src/esp_rtl_sdr.cpp`: `session_acquire`, `start_usb_stack`,
`host_lib_task_fn`, `worker_task_exit`, `esp_rtl_sdr_install`,
`esp_rtl_sdr_uninstall`, `session_release`; existing `stop_stream_internal`,
`drain_live_urbs`, `close_opened_device`, `retry_deferred_closes` remain relevant.

Baseline lifecycle: install creates synchronization/EP0 resources, acquires the
shared host and logical slot, registers a per-handle client and starts its worker.
The worker enumerates/probes and exclusively claims an address. Start configures
the profile, allocates buffers and submits URBs; stop pauses submissions and
drains completions before releasing the interface and pool. Final uninstall
formerly ignored join timeouts and client deregistration errors, decremented
session ownership before proving teardown, stopped host event processing without
`usb_host_device_free_all()`, ignored host-uninstall errors, and forcibly cleared
`live_urbs` even when another handle still owned the shared host.

IDF 5.5.4 `usb_host.h` requires clients deregistered and devices freed before
uninstall. `usb_host.c` calls lower layers with `ESP_ERROR_CHECK`;
`hcd_port_deinit()` requires zero queued/idle pipes. `usb_private.c` shows that
device/URB cleanup is asynchronous. This supports the reported cleanup-order
defect; it does not physically reproduce its abort.

After the change:

1. Initial host installation leaves the root port unpowered until its event task
   exists. Host-task creation failure cannot start enumeration in the interim.
2. Concurrent first installers publish one mutex using atomic compare/exchange.
   Waiting for another installer is bounded. Shared slot/address ownership stays
   intact until release succeeds.
3. Uninstall drains before stopping workers. Atomic exit counts establish their
   last access to handle memory; generic task notifications are no longer treated
   as proof of task exit. Timeouts retain the handle rather than free its memory.
4. Outstanding bulk/control transfers, deferred device closes and failed client
   deregistration prevent further teardown. Outstanding URB counts are never
   forcibly zeroed by uninstall.
5. Only the last owned-host reference requests device cleanup. The event task
   stays alive while waiting for ALL_FREE. Cleanup error/timeout prevents host
   task shutdown and host uninstall. A delayed ALL_FREE survives a retry.
6. Host-task join and host uninstall must succeed before clearing session state
   or releasing its final slot. New acquisition is blocked during incomplete
   final teardown. Non-final and externally owned sessions do not free devices
   or uninstall the host.

The external patch improved the missing free request, but continued after its
one-second timeout and did not address worker joins, forced URB clearing,
ownership/error propagation or concurrent final teardown. This implementation
does not reuse that patch.

### Failure contract and remaining risk

On uninstall error the pointer is a **cleanup-only handle**: retain it and retry
uninstall from its owner task; other handle APIs are disabled. Only ESP_OK permits
clearing the pointer. A failed install normally returns NULL, but if its rollback
cannot safely complete it returns a cleanup-only handle and logs that condition.
This is an intentional error-path contract change and requires consumer review.

Empty-list success from `usb_host_device_free_all()` cannot prove enumeration
idle; it is conservatively refused. The no-dongle case therefore loses successful
final teardown on this branch. The managed USB 1.6.0 source still uses assertions
in root-port shutdown, so an unproven power-off workaround was not added. Plugging
in another device during the ALL_FREE-to-uninstall interval remains unverified.
Hardware/SDK work is needed before claiming all supported uninstall cases pass.

The host tests compile the **actual** `session_acquire`, `SessionShutdown`, `session_release` and
`esp_rtl_sdr_uninstall` definitions with mocked platform calls. They test ordering,
state retention and retry, not HCD timing, enumeration itself, actual locking or
drain correctness. Initial shutdown-order regression failed, then passed. A new
callback-rejection/retry test caught and corrected a retry-flag bug during this
implementation. No sanitizer or real cross-core lifetime proof is claimed.

## B. Memory allocation

Affected source: `pull_ring_auto_prefer`, `ensure_pull_ring`, `PullHeap`,
`alloc_bulk_pool`, `ensure_ring`, and the allocation stage of `esp_rtl_sdr_start`.

The default six 16384-byte transfers give 98304 bytes of USB payload. Four times
that is **393216 bytes (384 KiB)**. The auto policy bounds that preference to
192000…524288 bytes. Baseline code tried that full ring in PSRAM, then internal
RAM, before reserving the transfer pool. Its shrink path only ran when the ring
allocation itself failed. Thus a successful oversized internal ring could starve
the later USB pool; this is source-confirmed, not a measured board heap failure.

| Resource | Placement / lifetime |
|---|---|
| Handle, transfer pointer array, descriptors | CPU metadata; general allocation; until teardown/pool release |
| EP0 data (72-byte request) and bulk buffers | `usb_host_transfer_alloc`; IDF controls DMA/cache alignment; default internal DMA-capable RAM |
| HCD descriptor lists | IDF allocation/capability rules; not manually relocated |
| Client/host tasks and queues/semaphores | FreeRTOS-managed resources; host/client/delivery requested stacks 4/6/6 KiB, plus platform overhead |
| Six IQ delivery slots | CPU copies, default 6×16384 = 96 KiB; prefer PSRAM, internal fallback; retained until uninstall |
| Pull ring | CPU copies for READ/BOTH; prefer PSRAM; CALLBACK does not allocate it; retained until uninstall |

ESP32-P4 can support USB DMA in PSRAM through IDF's explicit
`CONFIG_USB_HOST_DWC_DMA_CAP_MEMORY_IN_PSRAM` capability/cache path. This branch
does not enable it or substitute ordinary PSRAM buffers for DMA transfers.
Both IDF 5.5.4's `usb_private.c` and USB 1.6.0 distinguish these capabilities.

Required URBs now allocate before IQ slots and the pull ring, still before any
submission. PSRAM keeps the existing preferred capacity. Internal auto fallback
uses the current largest internal block with the existing 32 KiB reserve, then
tries smaller even capacities down to 64 KiB. A constrained-heap regression
verified that 96 KiB failure still attempts the 64 KiB floor. Explicit ring
sizes are never silently reduced. Failure returns NO_MEM; logs identify the
failed transfer index/size, free internal heap/largest DMA block, or ring budget.

An extracted baseline startup stage fails under a simulated 300 KiB internal
heap; the modified stage passes with required transfer allocation first. This
isolates the ordering regression using mocked allocations, not a physical heap.

The reserve is a heuristic for the 6 KiB delivery stack, synchronization and app
headroom, not a measured guarantee against other allocators or multiple future
receivers. A 64 KiB ring buffers only about 34 ms at 960 kS/s or 14 ms at
2.4 MS/s. Shrinkage and approximate duration are logged; sustained throughput
and multi-device heap acceptance remain physical gates. Existing reusable
CPU rings can stay allocated after failed startup until uninstall; this is
retention, not a promise that every failed start restores the pre-start heap.

## C. Smoke portability

`examples/p4_serial_smoke/main/main.cpp` formerly always initialized Tab5 I2C
and power expander 0x44. There was no board-selection gate, so another board
could fail this check despite a working driver.

New explicit TAB5/GENERIC/UNKNOWN Kconfig profiles avoid runtime guessing.
The existing default image explicitly remains Tab5/pre-v3. A separate generic
defaults file avoids carrying Tab5 power and silicon choices onto another board.
TAB5 executes the existing check (broken hardware still FAILs); GENERIC prints
SKIP/NOT_APPLICABLE; UNKNOWN prints SKIP/NOT_VERIFIED. Skips have a separate
summary counter and never increment passes. Host tests execute the actual row
and board-selection functions for all three profiles. A power-latch result is
not a measurement of loaded VBUS.

## D. AUTO gain/clipping

No confirmed gain defect and no register/default change. The Blog V4 AUTO path
`apply_tuner_agc_auto_records` uses the first-party measured AUTO triplet
`E8/78/6B`, composed through `run_band_frontend`; the manual ladder and RTL
digital AGC use distinct records. Existing capture documentation records tuner
AUTO SHA-256 `E131C5C642E7DFE8443D4667975871C220FFF4A237826B8423E9E38944BD3E8E`
and RTL AGC SHA-256 `1E5B006179C548E62617F530D5BC292AD45900763F6BCEF357264D4FDD55D482`.
Their large binary captures were not re-read or remeasured.

Przemek's 55% result remains unverified here; neither expected AGC behavior nor
an implementation defect is established by it. Getters report requested state,
not applied-register readback. Measure identical board/dongle/antenna, RF level,
frequency, rate and bandwidth; record settled AUTO, multiple manual steps and RTL
AGC independently, before/after attenuation, with clipping fraction and I/Q
distribution. Capture successful control writes and compare the same conditions
against the first-party desktop reference. Keep bias tee and input power known.

## Regression and physical acceptance matrix

PASS below is bounded to the named host test. Hardware for every row is NOT RUN.

| ID | Scenario | Host evidence | Remaining physical / SDK gate |
|---|---|---|---|
| USB-01 | No dongle | PASS: empty-list error retains session | Final clean teardown intentionally unavailable; release blocker |
| USB-02 | Attached idle | PASS: free→ALL_FREE→join→uninstall | Attached-dongle HCD acceptance |
| USB-03 | Stream/stop/uninstall | PASS: mocked drain gates destruction | Real drain/completion ordering |
| USB-04 | Enumeration fails | PASS: ambiguous empty list does not uninstall | Actual enum failure injection; clean teardown unresolved |
| USB-05 | Disappears during enumeration | NOT RUN | Actual event/pipe cleanup |
| USB-06 | Unplug during stream | PASS only for retained outstanding-count gate | Existing unplug path and blocking-read behavior not physically retested |
| USB-07 | Repeated stop/start | NOT RUN as streaming workload | Rate changes, late completions, heap after each cycle |
| USB-08 | Repeated install/uninstall | NOT RUN; not promoted to supported | Existing IDF/Tab5 re-entry limit remains |
| USB-09 | Shared handles | PASS: nonfinal release leaves event task/host alive | Four-dongle stream and heap test |
| USB-10 | One handle releases | PASS: no global free/uninstall on nonfinal release | Another handle's uninterrupted IQ |
| USB-11 | Cleanup fault/timeout | PASS: free/join/deregister/uninstall failures retain and retry | Real timing and SDK error behavior |
| USB-12 | Partial initialization | PASS: actual acquire install/task-create/power/rollback failures and no-session release | Real client-registration failure and complete empty-host cleanup remain pending; cleanup-only handle may require reboot |

| Memory scenario | Status and scope |
|---|---|
| PSRAM, preferred auto capacity | PASS: injected allocator keeps 384 KiB and avoids internal fallback |
| No PSRAM, bounded auto capacity | PASS: injected internal budget; real sustained streaming NOT RUN |
| READ / BOTH | PASS: actual startup stage with READ and BOTH; mode-specific streaming NOT RUN |
| CALLBACK | PASS: actual startup stage skips pull allocation; streaming NOT RUN |
| Small explicit ring | PASS: 1 KiB allocated without auto minimum |
| Oversized explicit ring | PASS: injected failure, no silent shrink; public validation caps at 1 MiB |
| Constrained/fragmented internal heap | PASS: budget, retries, 64 KiB floor and below-floor rejection |
| Failed allocation / cleanup | PASS: actual startup stage with injected allocation failures; physical heap matrix NOT RUN |
| Repeated start/stop / throughput | NOT RUN on hardware; no new performance claim |

## Verification commands and evidence

Logs are retained locally under ignored `build/evidence/`; binaries/configs under
isolated `build/` subdirectories. Do not confuse local logs with published lab evidence.

| Gate | Actual command / result |
|---|---|
| Baseline Windows | `tests/scripts/run_host_tests.ps1`: PASS, original 6/6 suites |
| Baseline P4 | IDF 5.5.4: `idf.py -C examples/p4_serial_smoke -B build/baseline55 build`: PASS |
| Windows regressions | `cmake --build tests/host/build --config Debug`; `ctest --test-dir tests/host/build -C Debug --output-on-failure`: final result below |
| Linux regressions | `cmake -S tests/host -B build/host-linux -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror"`; build + CTest: PASS, 13/13 at first expanded run |
| Existing runtime trace | WSL `python3 tests/scripts/test_nooelec_runtime_trace.py`: PASS, 7990 checks |
| Truth/version hygiene | Git Bash `tests/scripts/check_truth_hygiene.sh`: PASS, version unchanged at 0.9.3 |
| Modified Tab5 P4 / 5.5.4 | `idf.py -C examples/p4_serial_smoke -B build/changed55 build`: PASS before final verification refresh |
| Modified Tab5 P4 / 6.1 | `idf.py -C examples/p4_serial_smoke -B build/changed61 build`: PASS before final verification refresh |
| Generic P4 / 6.1 | `idf.py -C examples/p4_serial_smoke -B build/generic61 -D SDKCONFIG_DEFAULTS=sdkconfig.generic.defaults build`: result recorded below |
| Hardware / flashing | NOT RUN; no serial sessions or firmware writes |

The first baseline build invocation used a relative SDKCONFIG path that IDF
resolved under the app directory and failed to configure. Retrying with the
project's existing per-build SDKCONFIG rule succeeded. This was an invocation
error, not a baseline driver failure. Initial 6.1 compilation found volatile
increment deprecations under its newer default dialect; explicitly selecting
the driver's existing C++17 target fixed that compatibility failure. Windows
uninstall-harness compilation initially needed an MSVC atomic-read shim; this
does not simulate actual concurrency. IDF/third-party headers emit warnings,
including the legacy I2C deprecation; they are not silently suppressed.

## Compatibility and review

Tab5 retains its explicit power test and current profile behavior. Waveshare and
other boards can select GENERIC without touching Tab5 I2C. Systems without PSRAM
reserve transfer memory first; lower buffering latency tolerance is observable.
Single/multiple handles use the same session ownership rules; four-dongle physical
acceptance must be repeated before adoption. Existing OrcSDR consumers/pins are
unchanged, but callers must handle retained pointers on teardown/rollback errors.

CI adds a 6.1 compile lane beside 5.5.4. The root component and both existing
example wrappers carry a conditional, pinned USB 1.6.0 manifest for IDF >=6.0;
5.5.x continues using its bundled library. No IDF installation was upgraded or
modified. The 5.5.6 compatibility lane remains unavailable locally.

A focused read-only reviewer found no confirmed blocker in the inspected code,
but identified the unproven late-attachment window and the mock/concurrency limits
noted above. No CodeRabbit, Grok, remote CI or physical review is claimed.

## Release-note draft and attribution

Unreleased maintenance candidate: prioritize required USB transfer allocation,
bound internal automatic pull-ring allocation, add allocation diagnostics and
explicit smoke board profiles, and retain resources when USB cleanup or worker
shutdown cannot be proven. Add host lifecycle/allocator/smoke regressions and an
ESP-IDF 6.1 build comparison while retaining 5.5.4. Findings and external test
reports by **Przemek Godek — https://github.com/pgodek** informed this independently
implemented work. No external implementation code was reused. Hardware
acceptance, enumeration-idle teardown and concurrent attachment remain open;
this is not a release and does not claim every uninstall abort is resolved.

## Before considering a release

1. Resolve the empty-list/enum-pipe ambiguity and late-attachment window against
   named SDK revisions; do not weaken the failure gate merely to get smoke PASS.
2. On separately authorized Tab5 and Waveshare units, record board/silicon,
   VBUS configuration, firmware/source hash, SDK/USB version, dongle and antenna.
3. Execute USB-01…12 where supported, including attached-idle, failed enumeration,
   unplug during enumeration/stream, injected cleanup timeouts, and another
   dongle attaching during final teardown. Record failures without forced cleanup.
4. Compare PSRAM on/off at default and explicit capacities in READ/CALLBACK/BOTH;
   record free/largest DMA heap, allocation errors, overruns, drops and effective
   sample rate over repeated starts and a long soak. Test four concurrent dongles.
5. Perform the controlled AUTO/manual/RTL-AGC RF comparison described above.
6. Review error-path consumer handling, then open a review lane if desired.
   No merge, tag, version bump or release is authorized by this report.

## Final verification refresh

PASS on the final source snapshot:

- Windows: 14/14 CTest suites (`build/evidence/final-host-test.log`).
- Linux/GCC with warnings as errors: 14/14; Nooelec trace 7990 checks
  (`build/evidence/final-linux.log`).
- IDF 5.5.4 Tab5 compile (`build/evidence/final-idf55.log`).
- IDF 6.1 Tab5 compile (`build/evidence/final-idf61.log`).
- IDF 6.1 GENERIC compile (`build/evidence/final-generic61.log`).
- Truth/version hygiene: PASS, still 0.9.3 (`build/evidence/final-truth.log`).
- `git diff --check`: PASS. Upstream refreshed before delivery; base still current.

Verified build configurations: Tab5 min-revision full value 0; GENERIC value 301;
all three images have PSRAM disabled. These are compile configurations, not
identified physical boards. No PSRAM-enabled firmware or IDF 5.5.6 compile was
run; the PSRAM allocation path was exercised through injected host heaps.

The final focused follow-up review found no new actionable correctness finding
in session acquisition/rollback. It is not a full audit or hardware approval.

| Image | Bytes | SHA-256 |
|---|---:|---|

| `changed55` | 391536 | `fa99b9130210ede73f53c2b4b84c30e9e52c75966b1ff65bb0cf4d3808d582b2` |
| `changed61` | 376816 | `7e10b6becd219d49963f807422c5adc11cca8ab16030a0661aa750d5d556260b` |
| `generic61` | 341248 | `3ab546220bea3a8bd49c8ffb64606505a58f99227e7a416206baa8502ee1df86` |

## Changed-file inventory

| File | Why changed |
|---|---|
| `.github/workflows/ci.yml` | Add IDF 6.1 alongside the 5.5.4 compile lane. |
| `CMakeLists.txt` | Compile the component explicitly as C++17 on newer IDF. |
| `PROJECT_TRUTH.md` | Record host/build evidence and unresolved release gates. |
| `docs/API.md` | Document cleanup-only handles and conservative empty-list refusal. |
| `docs/API_REFERENCE.md` | Update lifecycle error contracts and pull allocation policy. |
| `docs/TESTING_GUIDE.md` | Explain generated runtime regression harnesses and limits. |
| `docs/reviews/EXTERNAL_DRIVER_FINDINGS_2026-10-29.md` | Investigation, attribution, tests, release draft and acceptance gates. |
| `examples/multi_rtlsdr_test/components/esp_rtl_sdr/CMakeLists.txt` | Keep the stable example wrapper on the driver C++17 dialect. |
| `examples/multi_rtlsdr_test/components/esp_rtl_sdr/idf_component.yml` | Carry the conditional USB 1.6.0 dependency through the stable example wrapper. |
| `examples/p4_serial_smoke/README.md` | Board selection, skipped checks, SDK builds and teardown limitation. |
| `examples/p4_serial_smoke/components/esp_rtl_sdr/CMakeLists.txt` | Keep the stable example wrapper on the driver C++17 dialect. |
| `examples/p4_serial_smoke/components/esp_rtl_sdr/idf_component.yml` | Carry the conditional USB 1.6.0 dependency through the stable example wrapper. |
| `examples/p4_serial_smoke/main/CMakeLists.txt` | Make the explicit M5 dependency conditional on Tab5. |
| `examples/p4_serial_smoke/main/Kconfig.projbuild` | Explicit Tab5, generic and unknown power profiles. |
| `examples/p4_serial_smoke/main/main.cpp` | Gate Tab5 power access and count/report skipped checks. |
| `examples/p4_serial_smoke/sdkconfig.defaults` | Make the existing Tab5 profile explicit. |
| `examples/p4_serial_smoke/sdkconfig.generic.defaults` | Generic-board defaults without Tab5 power/silicon settings. |
| `idf_component.yml` | Add pinned, conditional managed USB dependency for IDF >=6. |
| `include/esp_rtl_sdr.h` | Document retained handles on failed cleanup and ownership rules. |
| `private/rtl_pull_alloc.hpp` | Bounded CPU pull allocation, exercised with injected heaps. |
| `private/rtl_usb_shutdown.hpp` | Small teardown-order routine shared by runtime and host tests. |
| `src/esp_rtl_sdr.cpp` | USB lifetime/rollback/error gates, DMA-first allocation and diagnostics. |
| `tests/host/CMakeLists.txt` | Register added suites and regenerate actual-source harnesses. |
| `tests/host/start_alloc_harness.cpp.in` | Mock heap around the actual startup allocation stage. |
| `tests/host/test_pull_alloc.cpp` | PSRAM/internal budget, explicit-size and floor regressions. |
| `tests/host/test_usb_shutdown.cpp` | Cleanup ordering and error propagation regressions. |
| `tests/host/uninstall_harness.cpp.in` | Actual uninstall lifetime/error/retry fault-injection fixture. |
| `tests/scripts/generate_lifecycle_test.py` | Extract actual session/uninstall/startup/smoke code into host fixtures. |
