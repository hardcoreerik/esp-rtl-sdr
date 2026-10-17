# Changelog

## Unreleased

### Docs

- Driver validated on a third ESP32-P4 board, the Waveshare `waveshare-p4-wifi6` (maintainer-reported, 2026-10-06). Blog V4, V4L and V3c each sustained 3.20 MS/s through a hub/squid cable, matching the other Waveshare board, in a 3-hour soak. The driver worked as a drop-in with no modification; only the OrcNode firmware's pin mappings changed for the new board. Host matrix updated; hub model, per-dongle vs combined soak duration and a filled log are still to be recorded.

## 0.9.3 (2026-10-02) — RF band select, unplug recovery, V4/V4L band select

### Fixed

- **Blog V4 and V4L follow the captured RF mux / tracking-filter bands above 28.8 MHz ([#51](https://github.com/hardcoreerik/esp-rtl-sdr/issues/51)).**
  Our 2026-10-01 vendor-DLL captures (47 tunes up and down on each board, plus 10 kHz scans of every edge) show both boards stepping register 1b
  through the same rows as the R820T2 table, keyed on the LO the PLL uses. The V4 route wrote only `00` or `34` (the V4L `34`), so e.g. 116-306 MHz ran
  with the 96 MHz filter. The V4 also toggles register 17 bit 3 at 85, 112, 172 and 242 MHz (RF-keyed, independent of sample rate); the V4L does not.
  HF (28.8 MHz and below) is unchanged, and FM and ADS-B get the registers they always did. On a Tab5 the V4 gains about 3-6 dB peak over noise at
  230 MHz; the V4L is neutral for signal-to-noise (its level rises about 3.5 dB there); nothing else moved and nothing regressed
  (`docs/captures/tab5_rate_matrix_2026-10-01/`). Re-checked on the final code at 2.4 MS/s, same dongle and antenna, against the same build without this change:
  V4 at 230 MHz 9.3-9.4 to 13.0-13.5 dB over noise; 118.9, 162.55 and 453.925 MHz unchanged. One site, one antenna, one unit of each dongle.

- **R820T2 RF mux and tracking filter now follow the tuned band (Blog V3, Nooelec SMArt v5).** The tune sequence the driver replays was captured on FM,
  so every tune left registers 17, 1a and 1b on the 90-110 MHz setting and the front end was effectively deaf away from FM: the stream ran but heard
  nothing ([#25](https://github.com/hardcoreerik/esp-rtl-sdr/issues/25)). The issue was found and reported by David Coulson
  ([@davidcoulson](https://github.com/davidcoulson)), whose PR #45 pointed at the cause and informed our investigation; the implementation is ours, built
  from our own black-box PC captures of the vendor DLL (47 tunes up and down on a Nooelec SMArt v5 and a Blog V3c, both sides of every boundary), which
  give a 15-row band table keyed on the LO the PLL is programmed to (`private/r820t2_band.hpp`). Checked on a Tab5 (ESP32-P4) with a Nooelec SMArt v5
  and a dipole, same gain and antenna before and after: 453.925 MHz went from about 17 to about 46 dB peak over noise (14-16 to 49-66 strong bins),
  915 and 1090 MHz up 5-15 dB; on a Blog V3c 453.925 MHz went from about 1 dB to 40-45 dB. Blog V4 and V4L are unchanged by this entry.

- **Unplugging a dongle mid-stream, a dongle that fails to enumerate, and oversized or short control transfers are now handled.**
  The problem was reported by David Coulson ([@davidcoulson](https://github.com/davidcoulson), PR #48, from his fork's #26): a dongle unplugged
  mid-stream, or one that failed to enumerate after a reset, left the driver and the USB host stack in a bad state until a reboot. His report and PR
  informed the investigation; this implementation was written independently from our own requirements and hardware tests, not taken from his code.
  On DEV_GONE the driver now claims the shared EP0 window, waits a bounded time for the bulk URBs to retire (flushing the endpoint twice, then leaking
  them rather than freeing them under the host controller), and only then closes the device; control transfers refuse a device that is gone, refuse
  requests that do not fit the buffer or read more than 16 bytes from the I2C passthrough, and treat a short read as an error. A failed enumeration is
  retried by power-cycling the root port with back-off (10, 20, 40, then 60 s) when no enumerated device is on the bus, and a device that does not
  expose the bulk IN endpoint is rejected at probe. New host tests: `tests/host/test_usb_guard.cpp`. Hardware (ESP32-P4): 10 unplug/replug cycles
  mid-stream and a 105 s unplug with the root port cycled on a Nooelec SMArt v5; hubs are untested.

- **Nooelec SMArt v5: every start after a manual gain failed on ESP32-P4.** v0.9.2 re-applied the
  explicitly applied gain (or tuner AUTO) inside the tuner reinit, before the I2C repeater was
  switched back on, so the device STALLed the write and `esp_rtl_sdr_start` returned `ESP_FAIL`
  (a burst of EP0 STALLs, state `IDLE`) until a reboot. The first start after power-up worked
  because the gain mode was still AUTO. The restore now runs once the repeater is on, in the start
  path and on the Q-to-native return. Found on a Tab5 with a Nooelec dongle (issue #42): v0.9.1
  restarted fine, v0.9.2 did not, and with this change repeated stop/start and retunes in MANUAL
  gain stream normally. The host trace test now checks that the reinit writes no gain.
- **Low-band sample rates.** `esp_rtl_sdr_quantize_sample_rate()` masked the
  RTL2832 resampler field with `0x0ffffffc` and converted that field back to
  Hz without mirroring bit 27 into bit 28. Every request from 225001 Hz to
  300000 Hz was reported at about 2× (`250000` stored as `562500`), and
  `start()` programmed the demod from that wrong rate. `900000` Hz is now
  rejected: its stored field is `0x08000000`, which the same mirror realizes
  as 300 kHz. Rates from 900001 Hz through 3.2 MHz, including the 960 kS/s
  and 2.048 MS/s paths, were already exact. Host tests require the low-band
  rates to round-trip. Found and diagnosed by David Coulson ([@davidcoulson](https://github.com/davidcoulson)) in #24,
  including the correction that the stored field must keep the 28-bit mask and only the Hz calculation applies the mirror.

## 0.9.2 (2026-09-30) — capture-derived Nooelec SMArt v5 profile

### Nooelec NESDR SMArt v5 profile

- **Correct cold FM tuning on the Nooelec.** In v0.9.1, the Nooelec PLL calculation
  uses a 3.570 MHz IF but the shared initialization ends at a 1.815 MHz demod IF.
  The resulting +1.755 MHz shift is consistent with the reported 99.1 MHz station
  received at a 97.3 MHz dial setting. Restore the independently captured Nooelec
  `d3/6b` tuner filters and `38 11 12` demod IF at cold start and after returning
  from Q sampling. This source diagnosis and fix still require OrcSDR RF acceptance.
- **Implement the independently captured Nooelec controls.** Reuse the 29-step
  nominal native/FM gain ladder; preserve gain nibbles for tuner AUTO; add RTL AGC
  settle reads and seven native tuner-bandwidth choices at 2.4 MS/s. Explicit
  bandwidth AUTO follows the new Nooelec `c3/8f`, 1.815 MHz capture, while the older
  V3c AUTO boot-state policy remains specific to V3c.
  Retunes and bandwidth changes preserve Nooelec cold/manual/AUTO register-0c
  state, and returning from HF restores an explicitly applied gain/mode.
- **Add Q-branch HF tuning for this board.** Accept the manufacturer's model range
  of 100 kHz–1750 MHz, using the captured PC Q route below 24 MHz and restoring the
  native tuner above it. The 24 MHz PC tune reported no PLL lock; native RF operation
  there is unverified and the manufacturer rates native operation from 25 MHz.
  Tuner gain/mode and tuner bandwidth setters reject Q mode. Bias tee and HF
  upconverter capabilities remain absent.
- **Document provenance and acceptance separately.** The detailed
  [capture record](docs/captures/nooelec_v5_2026-09-30.md) records the USBPcap/TShark
  campaign, hashes, antennas, control/frame anchors and remaining capture gaps.
  [Merge notes](docs/nooelec_v5_merge_notes.md) record validation and the P4
  standalone driver acceptance procedure. The implementation was reviewed and
  merged in [PR #39](https://github.com/hardcoreerik/esp-rtl-sdr/pull/39), with
  Linux/Windows host tests, truth hygiene and a full ESP-IDF 5.5.4 P4 compile
  passing. Runtime traces passed 7,664 checks; physical Nooelec acceptance remains
  open. The maintainer explicitly selected version/tag `0.9.2` / `v0.9.2`.
  [Release notes](docs/releases/v0.9.2.md) explain the fix, evidence and limits.

## 0.9.1 (2026-09-28) — live tuner-bandwidth fixes; first published 0.9 release

`0.9.0` (below) was never tagged or published; `0.9.1` is the first published
0.9 release and contains everything listed under `0.9.0` plus the fixes here.
Hardware evidence for these fixes is on the M5 Tab5 with a V3c, V4L and V4
(`docs/captures/v3c_live_bandwidth_2026-09-28.md`).

### Fixed

- **Live tuner bandwidth no longer shifts the received frequency (V3c, V4, V4L).** The bandwidth
  transaction wrote the RTL2832 IF bytes 0x19/0x1a/0x1b back to back, while the captured init
  IF sequence and a PC live-bandwidth capture of the same V3c read page 0x0a reg 0x01 after
  every demod write. On the M5 Tab5 the V3c landed about +95 kHz off at 200 kHz and about
  -95 kHz off back at AUTO, and the V4L -312, -5, +98, -124, -50 and +386 kHz through 200k,
  300k, 500k, 1.0M, 1.8M and 2.4M, while the UI still reported 96.1 MHz. The sequence is now one
  helper, used by every profile, that reads after each IF byte; with it every stage of AUTO,
  200k, 300k, 500k, 1.0M, 1.8M, 2.4M, AUTO stays within +/-2.5 kHz of the request on the V3c,
  V4L and V4 (cold boot and unplug/replug; the largest is -2.5 kHz, V4L after replug). The V4 was not measured before the fix. The RTL2832
  mechanism behind the read is not established.
- **V3c AUTO tuner bandwidth restores the boot filter state.** AUTO paired the PC-capture
  filter registers 0x0a/0x0b = c5/8f (the PC's state with a 1.815 MHz IF) with the 3.570 MHz
  boot IF, leaving the left side of the passband about 13 dB down. The V3c boots with
  0x0a/0x0b = d5/6b (read back from the chip); AUTO now restores d5/6b with 3.570 MHz and IF
  word 38 11 12. The explicit 2.4 MHz plan and the V4/V4L plans are unchanged. Evidence:
  `docs/captures/v3c_live_bandwidth_2026-09-28.md`. Tuner-bandwidth plan values are
  vendor-driver control choices derived from captures, not measured analog passbands.
- **`esp_rtl_sdr_get_tuner_bandwidths()` no longer reads past the end of a table on the direct HF
  route.** On a V4 or V4L with the direct HF route enabled (for example CB at 27.205 MHz), the
  count said 7 widths but the values were read from the 4-entry HF list, so the last three were
  whatever sat next to it in memory. The count, the list and the plan lookup now come from one
  function (`measured_tuner_bandwidth_list()`), and a host test checks that every width offered is
  accepted by the plan lookup on every profile, RF and route. Found by CodeRabbit's review of #29.

### Documentation

- Capability matrix: documented the optional V4/V4L direct HF route and its two API calls, and
  updated the tuner-bandwidth row (found by CodeRabbit).
- The 0.9.0 summary no longer implies more V3c verification than was done.
- New `docs/VERSIONING.md` (how numbers are chosen, how releases are cut, how consumers pin the
  driver) and `docs/releases/v0.9.1.md` (plain-language release notes).

### Known issues (not fixed in 0.9.1)

Each needs a hardware retest before the fix can ship, so they are tracked rather than rushed:

- [#32](https://github.com/hardcoreerik/esp-rtl-sdr/issues/32) `hf_direct_min_hz` can be changed
  while streaming without retuning the tuner.
- [#33](https://github.com/hardcoreerik/esp-rtl-sdr/issues/33) A V4L bias-T change can overwrite
  the active route's GPIO bits.
- [#34](https://github.com/hardcoreerik/esp-rtl-sdr/issues/34) The width list and the upconverter
  rule disagree at exactly 28.8 MHz.
- [#35](https://github.com/hardcoreerik/esp-rtl-sdr/issues/35) A fifth simultaneously stuck device
  close is dropped.
- [#36](https://github.com/hardcoreerik/esp-rtl-sdr/issues/36) Nooelec SMArt v5: the PLL uses the
  3.57 MHz IF but the demodulator IF is not restored after init. The profile is provisional and
  has never been tested on Nooelec hardware here.
- [#24](https://github.com/hardcoreerik/esp-rtl-sdr/issues/24) and
  [#25](https://github.com/hardcoreerik/esp-rtl-sdr/issues/25) (David Coulson) remain open, with
  the fixes proposed in [#26](https://github.com/hardcoreerik/esp-rtl-sdr/pull/26).

## 0.9.0 (2026-09-26) — V4L, V3c and V4 hardware-verified; pre-1.0

The Blog V3/V3c and V4L profiles are no longer provisional for what has been
exercised on the M5 Tab5: streaming, hotplug and swaps, the measured tuner/gain controls, and
the V3c matched-IF FM tuning (see PROJECT_TRUTH.md for the exact scope). V3c absolute gain
accuracy, sensitivity and analog filter passbands are not established. The Nooelec
SMArt v5 profile stays provisional: no hardware has been tested. This release
also carries the concurrent multi-receiver foundation below.

### Added

- **Blog V4L and V4 direct HF route for 24-28.8 MHz** (`esp_rtl_sdr_set_hf_direct_min_hz`,
  `esp_rtl_sdr_get_hf_direct_min_hz`). Through the 28.8 MHz upconverter, a
  strong MW station at f also appears at 28.8 MHz - f (the LO's second
  harmonic), so with an MLA30+ the 1600 kHz station was heard on CB channel
  20 (27.205 MHz) on both V4L and V4. With the route enabled the R828S tunes
  RF directly (tuner = RF, native GPIO/input/bandwidth tables; the V4 uses its
  VHF input), as the V3c already does above 24 MHz. Off by default; cleared on
  attach. Tab5 on-air: CB channel 20 went from the 1600 kHz image to clean
  band noise on both V4L and V4, matching the V3c. Direct-input passband and
  CB sensitivity below 28.8 MHz are not measured.

### Fixed

- **A skipped device close no longer loses the handle.** When a control
  transfer never completes, `close_device_safely()` must not call
  `usb_host_device_close()` (it asserts), and it used to drop the handle.
  ESP-IDF keeps a gone device alive while it is still open, so repeated bad
  disconnects could pile up device objects and block later hotplug. Skipped
  closes are now parked and retried by the client task once the control path
  is idle, with a last attempt before the client deregisters on uninstall.
- **A halted bulk IN endpoint no longer kills the stream.** A transfer error
  halts the endpoint, after which every `usb_host_transfer_submit()` on that
  pipe fails. `bulk_cb` responded by setting `streaming = false`, and since
  all URBs share that flag one failed resubmit retired every one of them -
  permanently, while `esp_rtl_sdr_get_state()` still reported
  `STATE_STREAMING`, so nothing upstream could see it. `bulk_cb` now flags
  recovery and the delivery task clears the endpoint and resubmits via
  `bulk_resume()`; the clear must happen on a task, not in the transfer
  callback. Gives up after 8 consecutive attempts with no data so an
  unplugged device still surfaces as stopped.
- Rare with one dongle, common with two sharing a bus.

### Added

- `run_record()` logs the profile, request type, value, index, length and
  first data byte of any control record a device rejects, plus whether it
  STALLed. Profiles that borrow another board's init template will contain
  records their silicon does not accept, and `Dev N EP 0 STALL` from USBH
  does not say which - so closing those gaps meant guessing. Attribution,
  not invention.

### Changed

- **Blog V3 R820T2 streaming promoted from provisional to soak-verified**
  (2026-09-21). 1.62 GB at 1.024 MS/s over 787 s, concurrently with a Blog
  V4 at 2.4 MS/s behind an external USB hub: `usb_transfer_errors = 0`, IQ
  age never above 5 ms, measured 2.06 MB/s against 2.048 nominal, and zero
  init records rejected. It still runs the shared R820T2 template with the
  0x74->0x34 remap - there is no first-party V3 VHF/UHF capture - but the
  path is now evidenced rather than assumed. Nooelec shares the template,
  has no such soak, and keeps the provisional warning.

  Note for anyone chasing a similar fault: the EP0 STALLs seen during V3
  setup are the tuner-identification walk across I2C addresses and are
  expected. A Blog V4 throws six of them during its own probe and streams
  perfectly. They are not evidence of a bad init table.

### Added

- Shared USB host session (refcount) so multiple handles do not each call
  `usb_host_install` / uninstall under each other.
- Exclusive USB-address claim table: two handles cannot open the same dongle.
- `bind_device_index` / `bind_serial` config fields (`ESP_RTL_SDR_BIND_ANY`
  default keeps single-dongle behaviour).
- Identity, capture metadata, stream stats, hub stats APIs.
- IQ block append-only `device_id`, gain, bandwidth, flags. Timestamp remains
  USB-completion `esp_timer_get_time()`.
- `[RTLn]` log prefix on open / disconnect / bulk errors.
- Host tests: `tests/host/test_multi_device.cpp`.
- Harness: `examples/multi_rtlsdr_test/` with hub Kconfig enabled.
- Docs: `MULTI_DEVICE_ARCHITECTURE.md`, `WAVESHARE_P4_MULTI_RTL_PROTOTYPE.md`,
  `MULTI_DEVICE_PERFORMANCE.md`.

### Not in this change

- Anomaly detection, ML, novelty scores.
- Hardware-measured 2- and 3-dongle throughput (procedure only).
- Sample-sync / phase coherence (not claimed).

## 0.8.0-rc3 (2026-09-15) — V3c cold-start and LF/HF acceptance

### Fixed

- **Blog V3/V3c cold normal-tuner initialization:** cold starts at or above
  24 MHz now finish with the same captured R820T2/R860 reinitialization slice
  and tuner-repeater ordering already used by the verified direct-Q-to-normal
  transition. Raw 2.4 MS/s
  captures isolated the former path at tuner register state `0x05=0xE3` with a
  roughly 10 dB spectrum-half imbalance, while the transition path ended at
  `0x05=0x83` with a 0.431 dB median separation. The fix replays the captured
  sequence rather than hardcoding a register value. Post-fix V3c cold and
  direct-Q-return captures at 99.100 MHz were balanced with zero transport
  faults. V4, Nooelec, direct-Q,
  hot normal retunes, gain tables, and public APIs are unchanged.

### Changed

- **Blog V4 experimental LF policy (host/build-verified only):** custom tuning now
  preserves exact Hz and accepts 24 kHz through 1.766 GHz. Below 28.8 MHz the
  existing V4 upconverter mapping remains `tuner = requested RF + 28.8 MHz`, so
  DDH47 at 147.300 kHz maps to 28.947300 MHz.
- **Blog V3/V3c Q-branch LF/HF (capture-derived, experimental):** below 24 MHz,
  the driver now bypasses/shuts down the R820T2-family tuner, enables Q-branch
  direct sampling, and programs the captured exact-Hz RTL2832 NCO. Both hot
  transition directions reuse existing captured tuner cleanup/reinit records.
  A real V3c completed PC-side cold tunes and IQ reads at 60 kHz through
  23.999999 MHz plus normal/direct hot transitions. Tuner gain setters return
  `ERR_UNSUPPORTED` while direct sampling is selected. ESP32-P4 RF reception
  and DDH47 decoding remain open; Nooelec remains fail-closed below 24 MHz.

## 0.8.0-rc2 (2026-09-14) — EXPERIMENTAL multi-dongle

### Fixed

- **Blog V3/V3c manual-gain stage candidate (implementation and host/build
  verification):** the provisional table linearly interpolated two endpoint
  register pairs even though R820T2/R860 gain is selected by discrete LNA and
  mixer stages. A real V3c on a stable 99.1 MHz signal rose from -11.7 dBFS at
  0.0 dB to clipping near -1.5 dBFS, then fell to roughly -10 dBFS as the
  requested gain increased, proving the intermediate pairs were invalid. The
  29 advertised steps now use an alternating hardware stage candidate, and
  Blog V3 defaults to MANUAL because it does not expose measured tuner AGC.
  Mode changes now require the capability for the requested mode instead of
  accepting AUTO through manual-gain capability. Absolute gain values remain
  nominal until checked against a controlled RF source.
- **Tab5 release-image startup RAM regression:** the USB fault guard's timer
  handle and current-boot flag no longer consume ordinary internal BSS. The
  final RC4 image added only eight aligned bytes of internal state compared
  with RC3, but that crossed the ESP-IDF startup allocator's boundary and made
  the configured 40 KiB internal/DMA reserve fail before `app_main()`. The
  state now lives in the guard's existing RTC-retained record, preserving the
  full DMA reserve used by ESP-Hosted and audio. The failure was captured as
  `Could not reserve internal/DMA pool (error 0x101)` after M5Burner correctly
  loaded the application from `0x10000`; it was not a partition-layout error.
- **Blog V3/V3c matched-IF tuning (implementation, host/build, and physical
  hardware verification):**
  the earlier PLL-only repair set the R820T2/R860 tuner to the measured
  3.570 MHz IF, but the RTL2832 demodulator still finished initialization at
  the Blog V4-derived 1.814972 MHz IF. The 1.755028 MHz analog/digital mismatch
  explains why 96.1 MHz was heard near 94.33 and 99.1 MHz near 97.33 even while
  RDS identified the expected stations. After sample-rate programming and
  before the first PLL tune, Blog V3 alone now replays the captured RTL2832
  `0x19/0x1A/0x1B = 0x38/0x11/0x12` sequence, including the captured settle
  reads. Blog V4 emits no additional USB records and keeps its existing matched
  1.814972 MHz path; Nooelec receives no unverified override. On the exact
  candidate, a real V3c received the expected stations at displayed 96.100 and
  99.100 MHz; 99.1 also locked matching RDS. A subsequent live swap to Blog V4
  repeated both stations with the unchanged V4 IF and zero stream drops. V3c
  cold start, hot retune, reverse hotplug, USB-powered boot, and battery-powered
  boot also passed. Manual-gain calibration remains provisional.
- **USB fault-guard cleanup:** timer disarm now atomically claims the timer
  handle before stopping and deleting it, preventing concurrent expiry and
  device-discovery paths from tearing down the same timer. All early
  `esp_rtl_sdr_install()` failures now release their allocated mutexes and
  semaphores, including the safe-mode return path.
- **Host test false green:** both local runners and Windows CI now use CTest so
  both registered suites run. Before this correction the policy executable
  reported 373 passed while the profile executable was skipped; running it
  directly exposed one stale Blog V3 gain-capability expectation. That
  expectation now matches the existing capability behavior without changing
  gain code.
- **Root cause of the V3/Nooelec "not Hardware-verified" status, and of the
  reboot loop reported against real V3-family hardware ("One Blog V3 tester
  reported a reboot loop on insertion with RC1", 0.8.0-rc2 below; also
  reproduced independently against a physical unit sold as RTL-SDR Blog
  "V3c", R860 tuner per packaging): `probe_blog_v3_tuner()` tried to read
  the tuner's I2C chip-id register *before* the RTL2832U demod's own
  SYS/DEMOD bring-up had run. Proven with a PC/pyusb capture (not
  inference): the same chip-id read command genuinely STALLs — on a real
  PC, independent of ESP32-P4 — against **every** candidate tuner I2C
  address, including a real Blog V4's own correct R828D address (0x74),
  when sent cold. Replaying just `kRtlInitTransfers[0..86)` (the same
  demod-generic prefix that table already runs, unconditionally, before
  its own internal tuner auto-detect sweep at `kRtlInitTransfers[86..]`)
  first makes the identical probe succeed immediately after, on both V4
  and the V3-family unit. `probe_blog_v3_tuner()` now calls the new
  `run_demod_bringup()` (replays that same measured prefix) before its
  chip-id read. **This is why V3/Nooelec identification could never have
  been verified before**: the identification method itself could not
  succeed against any real hardware, independent of what tuner was
  actually attached. Confirmed hardware-verified after this fix: the V3c
  test unit now identifies as `blog_v3_r820t2` and streams, across
  repeated hot-swap (V4 ↔ V3-family, both directions) and cold-boot
  cycles, with zero crashes. The later matched-IF repair completed physical RF
  tuning verification at 96.1 and 99.1 MHz; manual-gain accuracy remains open.

### Added

- USB enumeration fault guard: `esp_rtl_sdr_install()` now tracks (via
  RTC-retained state) consecutive boots that panic while a device is
  attaching. After 3 consecutive enumeration-time panics it skips
  `usb_host_install()` entirely for that boot instead of retrying forever,
  returning the new `ESP_RTL_SDR_ERR_USB_SAFE_MODE`. Apps can query
  `esp_rtl_sdr_usb_safe_mode_active()` and clear the latch with
  `esp_rtl_sdr_usb_fault_guard_reset()`. Kept as defense-in-depth even
  after the real fix above: it still protects against a *different*
  incompatible stick hitting some other panic during enumeration in the
  future, turning an infinite reboot loop into "USB disabled this
  session" instead.

### Open hardware gates

- Blog V3/V3c tuning is now matched at 3.570 MHz in both tuner and demodulator.
  Hot retunes received the expected stations at displayed 96.100 and 99.100 MHz;
  99.1 locked matching RDS. Blog V4 then repeated 96.1 and 99.1 with its existing
  1.814972 MHz IF, matching RDS, and zero drops. V3c reattach restored the
  matched IF, 99.1 RDS, and a zero-drop stream. A subsequent battery-only cold
  boot also received the saved station correctly. That cold boot is
  user-observed because COM17 was necessarily absent; the sampled 96.1 V3c
  serial status had not yet acquired RDS lock even though reception was reported
  correct.
- Blog V3/V3c now has a discrete manual-gain stage candidate, but its stage
  order, absolute gain values, and RF response still require controlled-source
  hardware validation. Hardware tuner AGC remains unsupported; Nooelec gain and
  IF behavior remain unverified.
- OrcSDR (app-level, separate repo): auto-start-scan-on-attach did not
  trigger for the `blog_v3_r820t2` profile on hot-swap (manually
  navigating to the FM screen and back did restore audio/waterfall). Very
  likely gated on a V4-only capability bit or code path in the app, not a
  driver issue — needs OrcSDR-side investigation.
- R860 does not need its own profile: it is Rafael Micro's
  pin/register-compatible successor to R820T2 (same typical I2C address,
  0x34), so a genuine V3/V3c should continue to share `blog_v3_r820t2`
  once its tune/gain tables are corrected above — no new profile or
  `sdkconfig` entry needed (`sdkconfig` is ESP-IDF build config; it has no
  mechanism for per-tuner register math).

### Fixed

- Queues hotplug addresses for ordered processing by the USB client task in both USB-host ownership modes.
- Clears passport state on detach so a replacement dongle cannot inherit rate evidence.
- Keeps the Blog V4 initialization trace unchanged while excluding known V4-only board
  control values (`0x3001`, `0x3003`, `0x3004`) from provisional R820T2/R860 profiles.
- Adds profile-aware initialization, hotplug, and disconnect diagnostics.

### Known hardware status

- Blog V4 remains the non-regression baseline; RC2 hardware acceptance is pending.
- One Blog V3 tester reported a reboot loop on insertion with RC1.
- One Nooelec V5 tester reported static/intermittent waterfall but no received stations.
- V3 and Nooelec reception remain provisional and unverified by the maintainer.

## 0.8.0-rc1 (2026-09-10) — EXPERIMENTAL multi-dongle

### Added

- **Unified hardware profiles** (`Unknown`, `BlogV4`, `BlogV3`, `NooelecSmartV5`):
  driver-owned plug-and-play identity; apps consume
  `esp_rtl_sdr_get_profile()` / `esp_rtl_sdr_get_device_capabilities()`.
- **Provisional Nooelec NESDR SMArt v5** path: exact `Nooelec` + product contains
  `NESDR SMArt v5`; R820T2/R860 tuner I2C `0x34` remapping; RF < 24 MHz rejected;
  no Blog V4 HF Cable-2/GPIO5 routing; gain/bias CAP bits off (fail-closed).
  Contributor-tested; maintainer soak pending; community hardware soak welcome.
- **Provisional Blog V3 stream**: exact V3 descriptors or completed R820T2
  chip-id (`0x96`/`0x69`); `CAP_STREAM` true; shares evidence-backed R820T2
  I2C `0x34` IR remap with Nooelec (same USB template remapping only);
  RF < 24 MHz rejected; no V4 HF Cable-2/GPIO5; gain/bias CAP off.
  Experimental/community soak — **not** Hardware-verified / maintainer-unverified.
- Behavioral host profile tests (detection, unknown reject, tuner isolation,
  frequency policy, capability/transition matrix). Replaces PR #19 source-text
  scaffold checks.

### Changed

- Bare / unknown `0bda:2838` is **never** treated as Blog V4.
- Hotplug detach clears profile, device caps, and V4 front-end shadows (no
  cross-profile leakage).
- Experimental prerelease version **0.8.0-rc1** (does not silently replace
  stable 0.7.x).

### Preserved

- Blog V4 HF/VHF/UHF route composition from PR #18 / 0.7.15 remains the primary
  regression gate (Cable-2/GPIO5/Bias-T/gain). Physical V4 soak still open.

## 0.7.15 (2026-09-09)

### Fixed — Blog V4 HF hardware routing (hardware acceptance pending)

- Completes capture-derived Cable-2 (`R828D 0x06=0x38`) and RTL2832 GPIO5-low selection for HF; restores GPIO5-high and VHF/UHF input masks outside HF.
- Composes GPIO0 Bias-T state with GPIO5 routing and keeps `GPOE=0x39`, so Bias-T changes cannot undo the selected RF path.
- Composes manual/AUTO register-05 low bits with the HF/VHF/UHF input masks, preserving gain state across retunes and routing across gain changes.
- Uses the same complete route application for startup, hot retune, gain/AUTO, and Bias-T, and marks the applied route valid only after every required write succeeds.
- Selects the HF Cable-2 route at exactly 28.8 MHz while applying the 28.8 MHz LO offset only below that frequency.
- Host tests and ESP-IDF compilation do not constitute physical reception acceptance; GPIO, raw-IQ, AM, Shortwave, VHF, and UHF tests remain open.

## 0.7.14 (2026-09-07)

### Changed

- **ESP-IDF floor raised to 5.5.0** (CI builds `v5.5.4`, same floor as OrcSDR Tab5).

### Fixed

- **stop drains live URBs before free_bulk_pool (Tab5 HCD race):**
  `stop_stream_internal()` (used by `esp_rtl_sdr_stop`, e.g. POCSAG band-switch
  stop→start) previously halt/flush/clear'd, then took a fixed `bulk_num` ×
  timed semaphore, then unconditionally zeroed `live_urbs` and freed the bulk
  transfer pool. That could free a `usb_transfer_t` while IDF DWC HCD still had
  a bulk descriptor in flight → `_buffer_parse_bulk` assert
  (`desc_status != SUCCESS`) on Tab5. Stop now matches `bulk_pause_and_drain`:
  shared `drain_live_urbs()` polls `live_urbs`→0, halt/flush/clear only if still
  live, polls again, and **does not** call `free_bulk_pool` until `live_urbs==0`
  (on drain timeout: skip free, keep `pause_resubmit`, return
  `ESP_RTL_SDR_ERR_TIMEOUT`, state FAULT). `free_bulk_pool` also refuses while
  `live_urbs>0`. Uninstall may clear a stuck counter only after host teardown.
- **reset refuses while live URBs outstanding:** `esp_rtl_sdr_reset()` returns
  `ESP_RTL_SDR_ERR_BUSY` and keeps FAULT when `live_urbs>0` (after timed-out
  stop) so start cannot orphan the old transfer pool while callbacks may still
  fire; caller retries stop until drain, then reset.
- **uninstall does not free pool if host uninstall fails:** check
  `usb_host_uninstall()` return; only clear stuck `live_urbs` / `free_bulk_pool`
  after success; on failure leave pool, clear `destroying`, return ERR_USB.

## 0.7.13 (2026-09-05)

### Added

- **Public windowed metrics/health from metric snapshots:** apps take two
  esp_rtl_sdr_get_metrics() snapshots and call esp_rtl_sdr_metrics_delta() /
  esp_rtl_sdr_health_from_window() for scoped bytes, overruns, consumer drops,
  short transfers, effective SPS, efficiency, and USB health
  (OK / USB_STARVING / APP_TOO_SLOW) for that window only. Pure helpers
  (no new handle mutex state). get_health() remains lifetime/cumulative from
  stream start so early pull-ring overflow is not mistaken for soak drops.
  p4_serial_smoke soak scoring now wraps the public helpers.

## 0.7.12 (2026-08-27)

### Fixed

- **p4_serial_smoke SOAK row attributed pre-soak pull-ring overflow to the 8 s drain:**
  the harness waits 400 ms after `start()` before `first_read`, then starts
  `soak_drain`. At 960 kS/s CU8 those cumulative `consumer_drops` match an
  undrained ring, not the soak window. v0.7.11 Tab5 logs (507904 drops @
  `6x16384`, 593920 @ `3x32768`) were that artifact. Smoke now restarts the
  stream on the same USB host install (metrics and pull ring reset), drains
  immediately, and reports scoped/delta bytes, overruns, drops, and advice
  for the drain window. PASS requires efficiency >= 90%, delta overruns == 0,
  delta consumer drops == 0, scoped advice not `USB_STARVING` or
  `APP_TOO_SLOW`, continuing IQ, and overall hardware PASS. Driver streaming
  engine unchanged. Tag `v0.7.11` is immutable.
- **p4_serial_smoke A/B URB images shared one project-root `sdkconfig`:**
  IDF 5.5.4 `-B` does not move `sdkconfig`. Overlay `set-target` could make
  the later default image boot as `usb_soak_960k_3x32k`. Example now pins
  `SDKCONFIG` under the `-B` directory; docs use `build-6x16k` /
  `build-3x32k` with a pre-flash URB Kconfig grep. Smoke harness/docs only.

## 0.7.11 (2026-08-27)

### Fixed

- **p4_serial_smoke soak_drain stack overflow ([#11](https://github.com/hardcoreerik/esp-rtl-sdr/issues/11)):**
  drain_task allocated a 16 KiB local buffer on a 4096-byte FreeRTOS task
  stack (xTaskCreate sizes are bytes). Buffer is now a single static 16 KiB
  array. Smoke harness only; driver streaming engine unchanged. Tag `v0.7.10`
  is immutable. Re-soak both URB images against this tag.

## 0.7.10 (2026-08-27)

### Smoke / Tab5 USB

- `examples/p4_serial_smoke` quiet USB soak at 960 kS/s (BOTH + auto ring,
  helper `read()` drain, no mid-stream EP0). PASS if efficiency >= 90%.
  **One USB host install per boot.** A/B URB layouts are two images:
  default `6x16384` (`usb_soak_960k_6x16k`) and overlay `3x32768`
  (`usb_soak_960k_3x32k`, `sdkconfig.defaults.urb_3x32k`). Do not re-install
  the host in one run (IDF 5.5.4 `usb_host_uninstall` is not a reliable
  re-entry on Tab5).
- `sdkconfig.defaults` sets `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` plus
  `CONFIG_ESP32P4_REV_MIN_0=y` (example-only). IDF 5.5.4 hides rev 0 behind
  that parent symbol; without it Tab5 P4 v1.3 gets a v3.1 bootloader and
  esptool refuses. P4 rev <3.0 vs >=3.0 images are mutually exclusive.
- Example `PROJECT_VER` is `0.7.10` so the image identity is not git-describe
  of tag v0.7.9.
- The 0.7.9 Tab5 79% USB_STARVING figure mixed L4 gain/AGC bulk-pauses with a
  sparse BOTH `read()`. Those `consumer_drops` are not the starve signal.
  Hardware re-soak of both URB images is Tab5-operator work against this tag.

## 0.7.9 (2026-08-26)

### Fixed

- **Auto pull-ring `ESP_ERR_NO_MEM` on no-PSRAM Tab5:** default `pull_ring_bytes=0`
  still prefers ~4× URB (min ~192 KiB). If PSRAM/internal cannot allocate that,
  auto size now shrinks to the largest even internal block down to 64 KiB.
  Explicit `pull_ring_bytes` stays fail-closed. Tab5 L4 run 1 in
  `docs/Test_reports/ESP_RTL_SDR_0_7_8_DROP_IN_TEST_REPORT_2026-08-26.md`.
- **False `ERR_REENTRANT`:** reentrancy is the **callback task**, not a handle-wide
  depth flag. App-task setters no longer fail while the delivery task is emitting
  `EVT_IQ_BLOCK`. (Landed on 0.7.8 `master` untagged; first immutable tag is 0.7.9.)

### Smoke

- `examples/p4_serial_smoke` keeps real default auto ring (no 64 KiB override),
  suppresses IQ event logging, waits 3 s for enumeration, and emits L4 AUTO/RTL
  AGC rows. `sdkconfig.defaults` selects pre-v3 P4 silicon for Tab5 rev v1.3.
  Hardware re-run on COM17 is still required.

## 0.7.8 (2026-08-26)

### Added — measured Tuner AGC AUTO + RTL digital AGC

- **`CAP_GAIN_AUTO`**: `set_tuner_gain_mode(AUTO)` writes measured R828D IR trio
  `05=E8 07=78 0C=6B` (lab 2026-08-26). MANUAL restores the last ladder step.
- **`CAP_RTL_AGC`**: additive `set/get_rtl_agc` — demod `0x19` ON=`0x25` OFF=`0x05`.
  Not tuner AUTO. Apps that never call it are unchanged.
- Same async bulk-pause sideband queue as mid-stream gain/bias (0.7.6).
- Fail-closed: unclaimed → `ERR_NOT_CLAIMED`; callback → `ERR_REENTRANT`;
  same mode twice is a no-op. `set_tuner_gain` still forces MANUAL.
- After AUTO, skip triplexer filter rewrite (would clobber `0x0c=0x6B`).
- **IF / SDR# Bandwidth:** capture was USB-silent after open. `CAP_IF_FILTER` not added.

### Evidence

- `agc_tuner_on_off.pcapng` SHA-256 `E131C5C6…3E8E` (4 ON/OFF clusters)
- `agc_rtl_on_off.pcapng` SHA-256 `1E5B0061…D482` (4 demod 0x19 writes)
- `if_filters_steps.pcapng` SHA-256 `D9D32608…A9FB` (software-only Bandwidth)
- Procedure: `docs/AGC_IF_CAPTURE.md`

## 0.7.7 (2026-08-13)

### Added — Blog V4 HF upconverter CAP

- **`CAP_HF_UPCONVERTER`**: full advertised span **500 kHz … 1766 MHz**
- User RF **&lt; 28.8 MHz** programs R828D at **RF + 28.8 MHz** (public V4 SA612 path)
- Triplexer band FE after every tune/retune: HF / VHF / UHF (UHF keeps prior measured block)
- Helpers: `esp_rtl_sdr_frequency_uses_hf_upconverter()`, `esp_rtl_sdr_tuner_frequency_hz()`
- `NEED_HF` default LO = WWV **10.000 MHz** (was LO-only placeholder)
- Post-gain band filter refresh (does not clobber reg05 gain ladder)

### Evidence

- Offset + band edges: **public** RTL-SDR Blog V4 product page / datasheet (not GPL source)
- IR EP0 envelope: measured Blog V4 profile (`0x0074`/`0x0610`); HF reg05 family from init captures

## 0.7.6 (2026-08-13)

### Fixed

- Mid-stream gain/bias: **async queue** on delivery task (one bulk-pause window) so
  app/HTTP loop no longer blocks 1–3 s during EP0.
- Bias then gain in the **same** paused window when both pending; 40 ms settle after SYS bias.
- Gain IR writes: up to 3 full retries; control transfers: 3 attempts on STALL.
- Still pauses bulk before EP0 (same class of fix as retune).

## 0.7.5 (2026-08-13)

### Added — Phase 3 measured gain / bias (Blog V4)

- Clean-room tables from lab USBPcap (`private/measured_gain_bias_v4.hpp`)
  - Bias ON/OFF SYS sequence (`0x3004/3003/3001/3000` @ `0x0210`)
  - Manual gain ladder 0.0…49.6 dB via IR `0x0074`/`0x0610` pairs `{0x05,val}`, `{0x07,val}`, `{0x0c,0x68}`
- `set_tuner_gain` / `get_tuner_gains` / `set_bias_tee` apply measured EP0 when interface claimed
- **CAP_GAIN** and **CAP_BIAS_TEE** enabled (`MEASURED_2026_08_12`)
- AUTO gain mode still `ERR_UNSUPPORTED` (AGC path not in this capture set)
- Evidence report: `docs/PHASE3_CAPTURE_REPORT.md`

### Notes

- Multimeter SMA DC not recorded — bias electrical claim remains capture-level
- P4 re-soak of new CAP paths still open (lab)

## 0.7.4 (2026-08-13)

### Added

- **Delivery modes** (`config.delivery_mode`): `BOTH` (default) | `CALLBACK` | `READ`
  - `CALLBACK`: `EVT_IQ_BLOCK` only — no large pull-ring allocation
  - `READ`: blocking `read()` only — no `EVT_IQ_BLOCK`
  - `BOTH`: previous behavior
- **Lazy pull ring:** buffer allocated on first IQ push or first `read()` when mode uses read
- Optional `config.pull_ring_bytes` (0 = auto; even, 1 KiB…1 MiB)
- `CAP_DELIVERY_MODE` + pure helpers `delivery_mode_uses_callback_iq` / `_uses_read`
- `read()` returns `ERR_UNSUPPORTED` in CALLBACK-only mode

### Fixed (CodeRabbit on PR #6)

- Serialize `ensure_pull_ring` on the handle lock; fail-closed teardown of partial rings
  (no double-alloc race between delivery task and `read()`)

### Docs / process (from 0.7.3 review gap work)

- Full API reference, Kconfig/troubleshooting/examples, soak template, SCOPE, licensing
- Legacy `struct_size` fallback for delivery fields documented; versioning table through 0.7.4

## 0.7.3 (2026-08-12)

### Changed

- **True async retune from event callback:** `retune_hz` / streaming `set_center_freq`
  queue the LO and return `ESP_OK`; delivery task drains URBs, EP0 tunes, emits
  `EVT_RETUNED`. App-task calls still apply synchronously.
- Coalescing: newer pending LO while apply is in flight is not lost.

### Docs

- `docs/DEVELOPMENT_NARRATIVE_0_7.md` — verbose 0.7.x development commentary
  (architecture, CI meaning, hardening, async retune, lab honesty, open list)

### CI (carry-forward)

- ESP-IDF P4 compile gate for smoke example (5.3.2 + 5.4.1)

## 0.7.2 (2026-08-12)

### Hardening (review-driven)

- **STARTING** state serializes concurrent `start()`
- Deterministic **task join** on uninstall (notify, not fixed 50 ms delay)
- User callbacks use **atomic** `in_callback_depth`; `select_device*` emits **after** unlock
- **Transactional** IQ ring allocation / destroy on failure
- **struct_size** accepts min..sizeof (append-only ABI)
- **retune** from callback → `ERR_REENTRANT` (strict; true async later)
- **Kconfig** transfer size/count wired into `config_default` when built under IDF
- Pull ring uses **block memcpy** instead of per-byte loop
- `idf_component.yml` **targets: esp32p4**
- Docs: `docs/HARDENING_0_7_2.md`

### Fixed (carry-forward)

- Low-band min **225001 Hz** (not 225000) for ratio/desktop parity

### Version

- **0.7.2** (do not retag 0.7.1)

## Unreleased

### Tests / CI

- Expanded host policy suite (window edges, quantize idempotence, config matrix, names, CAP off)
- CI: Ubuntu + Windows matrix, `-Werror` on Linux, `ctest`, concurrency cancel
- `tests/scripts/check_truth_hygiene.sh` (version + required docs + CAP_GAIN/BIAS guard)

### Docs

- `docs/LAB_HOBBYIST.md` — honest hobbyist lab capabilities + TinySA Ultra how-to
- TESTING / GAIN_BIAS / PROJECT_TRUTH cross-links for desk-lab posture

## 0.7.1 (2026-08-12)

### Added

- Live **`EVT_HEALTH`** from delivery task (on overall change + every 48 IQ blocks)
- Phase 3 **API surface** (fail-closed): `set/get_tuner_gain_mode`, `set/get_tuner_gain`,
  `get_tuner_gains`, `set/get_bias_tee` — return `ERR_UNSUPPORTED`; **CAP_GAIN / CAP_BIAS_TEE remain off**
- `docs/GAIN_BIAS_CAPTURE.md` — clean-room capture procedure for lab (Baofeng/Flipper stimulus later)

### Notes

- Gain/bias preferences may be stored for apps; **no hardware effect** until measured EP0 lands.
- Version **0.7.1**.

## Unreleased

### Added (automated testing / TheOrc-aligned)

- Host unit suite: `tests/host` + `src/esp_rtl_sdr_policy.cpp` (no IDF/USB)
- Scripts: `tests/scripts/run_host_tests.ps1` / `.sh`
- CI: `.github/workflows/ci.yml` (policy tests + version/truth hygiene)
- `docs/TESTING_GUIDE.md` — layers, how to run, what is/isn't claimed

### Added (open-source honesty / TheOrc-aligned)

- `docs/AI_DEVELOPMENT_DISCLOSURE.md` — human-directed, AI-assisted; trust rituals
- `docs/DOCUMENTATION_STANDARD.md` — accuracy-first docs (no planned-as-done)
- `SECURITY.md` — vulnerability reporting
- `CONTRIBUTING.md` — clean-room + truth PR checklist
- `docs/README.md` — docs index
- `PROJECT_TRUTH.md` rename (was `Project_truth.md`) + development honesty table
- README credits + “where we are” honesty block

## 0.7.0 (2026-08-12)

### Added

- **Continuous sample rates** within hardware windows  
  (225–300 kHz ∪ 900 kHz–3.2 MHz) with `quantize_sample_rate` → exact SPS.
- **Intent:** `esp_rtl_sdr_apply_need()` — FM / ADS-B / WX / HF / MAX_STABLE / LISTEN.
- **Health:** `esp_rtl_sdr_get_health()` — USB/RF categories + advice string.
- **Passport:** `probe_rates` / `get_rate_passport` / opts default; progress events.
- Caps: `CONTINUOUS_RATE`, `NEED`, `HEALTH`, `PASSPORT`.
- Events: `EVT_HEALTH`, `EVT_PASSPORT_PROGRESS`, `EVT_PASSPORT_DONE`.
- Docs: `docs/VISION.md`, `docs/SILICON.md`, `docs/TESTING.md` (Heltec V4×2,
  Baofeng UV-5R, Flipper Zero lab notes); RATES rewritten for continuous policy.

### Changed

- `is_rate_supported` = in-window + quantizable (not fixed allowlist only).
- `get_supported_rates` returns **recommended** named rates (includes 2.56M).
- `set_sample_rate` / `start` store **exact** quantized SPS.
- Version → **0.7.0**.

### Notes

- `NEED_HF` stores preferred LO only; upconverter CAP still open.
- Passport requires attached Blog V4; NO_DEVICE without dongle.
- Gain / bias / adaptive URB remain later phases.

## 0.6.0 (2026-08-12)

### Added

- Phase 2: expanded recommended rates, ppm correction, multi-device select.
- `docs/RATES.md`, capability/docs updates.

## 0.5.0 (2026-08-11)

### Added

- Stand-alone **esp_rtl_sdr** rename, Phase 1 desktop-shaped API, smoke example,
  Project_truth / architecture / Roadmap, GitHub hardcoreerik/esp-rtl-sdr.
