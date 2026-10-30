# Repeatable driver and OrcSDR regression gates

Przemek Godek (https://github.com/pgodek) reported attached-dongle uninstall
abort and no-PSRAM internal RAM exhaustion. These are driver lifecycle and
allocation regressions, not antenna or decoder tests.

## Gates

1. Every software PR: existing host CI exercises actual session/uninstall and
   allocation paths, plus policy, acquisition and FIFO tests. Failure injection
   covers cleanup errors, outstanding resources and allocation failure. These
   mocks do not prove physical USB teardown.
2. Driver hardware fixture: use the standalone smoke/probe firmware to test
   attached-dongle uninstall, no-PSRAM allocation, stream continuity and tuning.
   Record safe teardown refusal separately from successful teardown and crash.
   The incomplete-enumeration SDK gate remains open. This is not yet a fully
   automated physical lifecycle fixture.
3. Application integration: the read-only runner below checks active IQ and
   driver loss counters on Tab5, and streaming/display progress on Hosyond.
   The Hosyond console does not expose equivalent loss counters, driver profile
   or bytes; these are not silently inferred. Two attached dongles do not mean
   both are concurrently validated by this application test.
4. RF: a tinySA through a 30 dB attenuator can supply repeatable tone and gain
   response checks; known Heltec packets provide a separate LoRa decoder gate.
   Tone, decoder and antenna acceptance are separate. Do not require a new
   broadcast/antenna survey for every change confined to USB lifecycle code.

## Local observation

Python with pyserial is required. Close any competing serial monitor first.
The tool checks USB serial identity before opening the port, leaves DTR/RTS
false, sends only read-only status queries and rejects protected COM17.

```powershell
python -m unittest discover -s tools/lab -p test_observe_orcsdr.py
python tools/lab/observe_orcsdr.py --board tab5 --port COM39 --usb-serial E8:F6:0A:E2:D6:03 --seconds 60 --output artifacts/tab5.json
python tools/lab/observe_orcsdr.py --board hosyond --port COM8 --usb-serial 5AE6034489 --seconds 60 --output artifacts/hosyond.json
```

Existing output files are preserved. Results contain selected status fields,
not peer payloads, credentials or arbitrary raw serial logs. A passing result
does not attest the installed driver's revision: build/pin provenance and flash
hash verification must be recorded by the deployment step separately.

## Optional hardware CI

`lab-observation.yml` is manually triggered only. It requires a provisioned
Windows self-hosted runner labelled `esp-rtl-sdr-lab`, Python/pyserial, attached
devices and the `orcsdr-lab` environment variables TAB5_PORT, TAB5_USB_SERIAL,
HOSYOND_PORT and HOSYOND_USB_SERIAL. Use trusted code on the hardware runner;
do not enable this workflow for untrusted pull-request execution.

It observes already-installed firmware and uploads JSON results. It does not
build, flash or establish that the device runs the selected GitHub revision.
No runner is installed or registered by this change, and hosted workflow
execution is not claimed until the lab is provisioned and a run succeeds.

## Standalone COM39 fixture with recovery

The optional workflow mode `standalone-com39` builds the checked-out driver
with PSRAM disabled, then runs tools/lab/run_standalone_fixture.py. It is
restricted to the enrolled COM39 USB serial E8:F6:0A:E2:D6:03, requires explicit
--allow-flash and both image hashes, and attempts recovery even after a partial
initial flash failure. It writes only the application at 0x10000. COM17 cannot
be selected. Do not run while the user is operating COM39 or another monitor.

Provision these additional lab environment variables: LAB_IDF_554_PATH,
LAB_TAB5_RESTORE_IMAGE, LAB_TAB5_RESTORE_SHA256, LAB_ESPTOOL_PYTHON,
LAB_ORCSDR_ROOT and LAB_PAIRING_KEY_PATH. The key variable is a local file path;
key contents must never appear in workflow arguments or artifacts. Python with
pyserial and a trusted OrcSDR checkout/serial runner are prerequisites. The
restored application is resumed to the fixture's documented LoRa configuration,
not an arbitrary user's previous settings. Only use this mode on the reserved
fixture device. The exact standalone image/config correspondence remains the
build pipeline's responsibility; the runner checks supplied hashes and config.

Recovery runs in a finally block but cannot be guaranteed after process kill,
host power failure or workflow hard timeout. Recovery errors fail the run and
require operator attention. Configure sufficient workflow timeout and preserve
the known-good image independently. No lab runner/environment was provisioned,
and hosted execution of the new standalone mode has not been verified.

Local live validation: the first automated standalone run passed 46 checks,
including quiet soak/uninstall, and restored the application image, but its
immediate CLI resume failed with a serial timeout. The result correctly failed.
A subsequent resume-only retry passed. A bounded CLI readiness probe was added
before resume; that exact revised end-to-end path has not been rerun because
COM39's settings subsequently changed externally. Its current settings were
left untouched. Unit tests exercise partial flash recovery, test/recovery error
separation, unhealthy logs and protected/wrong device rejection; eight local
host tests pass. This is not a claim of unattended hardware-CI acceptance.
