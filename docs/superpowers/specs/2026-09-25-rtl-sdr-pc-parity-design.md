# PC-measured RTL-SDR feature parity: driver design

Status: original design approved in chat on 2026-09-25; bias safety revised
per user feedback to use an enable-time warning and attachment reset rather
than a separate opt-in API. Implementation plan review remains open.

## Purpose and evidence

Extend the existing single `esp_rtl_sdr` API so OrcSDR can use PC-measured
controls on Blog V4L, Blog V4, and the user-identified V3c without treating
those boards as interchangeable. The immediate blocker is that V4L currently
rejects AM/HF below 24 MHz. The first-party PC record is
[`V4L_PC_ROUTE_2026-09-25.md`](../../captures/V4L_PC_ROUTE_2026-09-25.md):
it contains the route and IF traces, gain ladders, AGC and bandwidth control
observations, bias-tee no-load checks, raw-file hashes, and user-confirmed
V4L AM1280 voice audio. The earlier byte-identical V4L PC IQ stream later
recovered; its cause is unknown and is **not** a diagnosed firmware defect.

Success means a driver build and targeted host tests can describe and apply
the measured board-specific controls, then a separate Tab5 run can verify
V4L AM1280 audio/spectrum/retune and three-dongle hot-swap. A host test or
PC capture alone is not hardware acceptance. All three dongles remain one
library with profile-scoped capabilities; OrcSDR consumes capabilities and
API results rather than implementing separate RF tables.

## Architecture and staging

Keep the current profile, control-record, ring/pause, and sideband queue
architecture. Do not fork the driver or copy V4's front-end table into V4L.
Implement in three independently testable stages:

1. **V4L RF route and matched IF.** Define a V4L-only route plan from the
   captured R828S/GPIO records. Tuner I2C remains `0x0034`; V4/R828D remains
   `0x0074`. Below 28.8 MHz the tuner target is RF + 28.8 MHz; at exactly
   28.8 MHz its PLL target is native while GPIO remains the observed HF
   value; above it GPIO switches to VHF. Preserve V4's triplexer route and
   V3c's direct-Q HF path. Apply V4L's board route at cold start and retune,
   including the measured tuner input/filter state and bias-OFF GPIO
   defaults. Preserve the observed two-phase order: input writes before
   PLL programming, then route/GPIO/gain writes after it; the V4L trace did
   not show a hot reg-06 triplexer write. Keep tuner PLL and RTL demod IF
   matched to the selected tuner
   bandwidth; at the dashboard's 2.4 MS/s default the PC observed about
   1.815 MHz. Remove only V4L's below-24-MHz fail-closed rule after that
   route exists. Reject frequencies outside the driver-wide policy as before.
2. **Gain, AGC, and bias parity.** Use a profile- and band-specific gain
   composition: V4L manual `05/07` follows the captured 29-step ladder,
   with AM `f0..ff` versus FM `90..9f` register-05 family. V3c FM uses the
   same measured 29-step nominal ladder; V4 retains its own table. Return
   the matching table from `get_tuner_gains()` instead of always returning
   V4's count. Add the captured V4L and V3c tuner-AUTO and RTL digital AGC
   records behind correct capabilities, keeping tuner AGC separate from
   RTL digital AGC. V4L and V4 advertise bias control with safe default
   OFF and board-specific GPIO composition. The user-identified V3c's PC
   trace and meter reading support an explicit bias setter on BlogV3, but
   its generic USB descriptor cannot guarantee that every look-alike stick
   has the same hardware. Advertise `BIAS_TEE` as an available **manual
   control** on BlogV3, document that caveat, and require OrcSDR to warn
   when a user enables it on this generic profile. No bias state may carry
   to another attachment: clear the desired ON state on detach, device
   selection change, stop/cleanup, and uninstall, and initialize a newly
   claimed device with GPIO bias OFF before streaming. A physically
   unplugged dongle cannot receive an OFF command; loss of USB power
   removes its USB-powered bias output. Nooelec and Unknown remain
   unsupported.
3. **Tuner bandwidth API and live switching.** Add a capability, supported
   width query, set and applied-state query. `0` denotes automatic tuner
   bandwidth, not IQ sample-rate or software audio bandwidth. Accept only
   request values directly captured for the relevant route class: on the
   native tuner route (anchored at FM96.1), auto, 200, 300, 500, 1000,
   1800, and 2400 kHz; on the V4/V4L HF upconverter route (anchored at
   AM1280), auto, 200, 500, and 2400 kHz. Applying these route-class maps
   away from the measured anchor frequencies remains a hardware-validation
   item. A 300 kHz native-route request maps to the same measured
   filter setting as 200 kHz. On V3c's direct-Q HF path, return
   `UNSUPPORTED` for analog tuner bandwidth; its PC tuner writes while
   bypassed do not prove a passband. For a live request, serialize through
   the existing EP0 sideband/pause path, snapshot the previous applied
   filter, PLL, and demod IF, then write the new combination as one logical
   operation. On error, restore the prior combination before IQ resumes.
   If restoration fails, enter/report FAULT instead of resuming with an
   unknown IF. As with existing async controls, an accepted queued request
   is not an applied-success claim: expose requested and applied values
   distinctly and propagate the eventual error through existing status or
   health reporting. Retune and bandwidth requests must serialize so they
   cannot pair a new PLL with an old demod IF.

## Safety and compatibility

- Keep bias OFF by default and on stop/cleanup. Clear ON preference on
  detach and force OFF on the next claimed attachment; never silently
  restore ON after a hot-swap. Do not enable it during the user's
  independently powered MLA30+ reception setup.
- Capability bits describe what this driver can actually attempt for that
  board, not every feature named in a PC application. A setter still checks
  current mode and interface claim; V3c tuner controls that act on a
  bypassed HF tuner are not claimed as effective HF controls.
- Add public ABI fields only append-only, following the existing
  `struct_size` compatibility policy. Existing OrcSDR builds must still
  compile against their current API until they deliberately pin this update.
- Preserve current V4 route/gain/bias behavior and provisional Nooelec
  fail-closed behavior. BlogV3 bias advertisement means a user-requested
  GPIO control is available, **not** that voltage was verified on every
  generic Realtek descriptor. Do not treat nominal gain labels as calibrated
  dB or PC tuner bandwidth register choices as measured analog passbands.
- The PC vendor driver had a same-open AM bandwidth failure and a transient
  byte-identical V4L IQ stream. Neither gets silently reclassified as a
  proven hardware limitation or a fixed firmware problem. Log and surface
  errors; do not add speculative retries to hide them.

## Verification and handoff

Start each stage with focused host tests that fail under the current code:
V4L frequency boundaries/route/GPIO/PLL/IF, per-profile gain tables and
mode/capability safety, then bandwidth maps and rollback state transitions.
Run only the targeted host suite and component build unless a shared layer
change or failure justifies broader testing. Record exact commands and
results. Do not claim RF success from those checks.

After the driver changes are independently verified, provide the user a
Claude prompt for the **separate** OrcSDR update. It will name the exact
driver commit to pin and require a Tab5 build, V4L AM1280 voice and
spectrum, AM retune, gain/AGC/bandwidth/bias-off behavior, V4 and V3c
regression, and three-dongle hot-swap. Build, flash, audio/RF behavior,
hot-swap, and publication are separate gates. No flash, push, merge, or
OrcSDR edit is authorized by this specification alone.
