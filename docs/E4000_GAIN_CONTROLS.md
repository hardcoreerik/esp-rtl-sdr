# E4000 tuner gain candidate

This implementation uses first-party XTR PC API/USB controls. No vendor driver
source was imported. E4000 remains experimental; physical acceptance of this
new gain implementation is pending. Existing gain APIs are reused, with
`CAP_GAIN` and `CAP_GAIN_AUTO` advertised only for profiles implementing them.
RTL2832U digital AGC remains a separate unsupported E4000 capability.

Nominal manual steps, in dB:
`-1, 1.5, 4, 6.5, 9, 11.5, 14, 16.5, 19, 21.5, 24, 29, 34, 42`.
API values use tenths of dB; nearest selection clamps at either end and chooses
the lower step on an exact tie. The list returned by `get_tuner_gains` includes
the negative first entry. These are reference API requests, not calibrated
total analog gain. The PC RF matrix showed a plateau between nominal 29/34 dB.

Evidence: `captures/e4000_xtr_2026-10-31/` and the retained local
`tinysa-paused-matrix/gain-exact-controls.txt`. All 813 controls paired. The
older decoder assigned stages up to 20 ms early; the exact-boundary decoder
uses the actual API-start timestamp so trailing writes are not attributed to
the next step. Raw capture is unchanged. LNA register 0x14 codes are
`00,01,04,05,06,07,08,09,0a,0b,0c,0d,0e,0e`; mixer register 0x15 bit 0 is
zero except for the 42 dB step. Manual mode clears register 0x1a low nibble
and register 0x20 bit 0; AUTO selects 0x09 and 1 respectively. Observed mode
transitions include 0x59→0x50 and 0x01→0x00. Unrelated bits are preserved by
read/modify/write through the E4000 bridge address 0x00c8.

Cold start and retune restore the stored manual setting or AUTO mode after
tuning. Streaming requests reuse the existing deferred control transaction.
Each write failure stops a live stream in FAULT; no IQ resumes with uncertain
gain state. Idle failures return an error, and the next start reapplies state.
No additional IF-stage initialization is inferred here: captured tuner writes
0x16/0x17 exist, but matching total gain requires a separate RF comparison.

Focused checks run on Windows/MSVC:

```powershell
cmake --build tests/host/build --config Debug --target esp_rtl_sdr_profile_tests
ctest --test-dir tests/host/build -C Debug -R '^profiles$' --output-on-failure
python tests/host/build/generate_runtime.py
cmake --build tests/host/build/runtime-msvc/build --config Debug
./tests/host/build/runtime-msvc/build/Debug/trace.exe
```

Results: profiles 2,202 passed/0 failed; actual-function runtime/API trace
9,757 passed/0 failed. The runtime harness mocks USB reads/writes and locking;
it covers every step, reserved bits, AUTO/manual transitions, returned ladder,
failure before publishing applied gain, and live failure behavior alongside
legacy Nooelec controls. It does not prove electrical/RF behavior or real RTOS
timing. Firmware build, cabled gain sweep, AUTO response, FM/RDS, restart and
retune persistence remain physical gates before this feature is accepted.
