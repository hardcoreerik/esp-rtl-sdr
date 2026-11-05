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
