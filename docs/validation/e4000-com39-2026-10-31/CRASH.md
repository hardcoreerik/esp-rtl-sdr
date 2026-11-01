# COM39 USB host panic after Ham It Up setup

October 31, 2026. User reported the Tab5 COM39 blue-screen crash after
connecting the Ham It Up Plus v2 in upconvert mode. Exact trigger action and
converter USB-power topology are awaiting clarification. Do not infer that
the converter, AM decoder or a frequency request caused the crash.

## Confirmed dump evidence

- Driver candidate: `068efab99615fead03e8dd56239ff0f12a9ccdaf`.
- App SHA-256: `09b98810135ab15ffbe08fd0256ab76b34f38416d4cdf8f61e860558577bd5ac`.
- Core integrity and application-ELF hash match were checked with the installed
  ESP-IDF coredump loader. Hashes are in `crash-20261031.json`.
- Panic: `assert failed: usb_dwc_hal_chan_decode_intr
  /IDF/components/hal/usb_dwc_hal.c:519
  (chan_intrs & USB_DWC_LL_INTR_CHAN_CHHLTD)`.
- Stack: `panic_abort -> esp_system_abort -> __assert_func ->
  usb_dwc_hal_chan_decode_intr -> _intr_hdlr_chan -> intr_hdlr_main ->
  _interrupt_handler`.
- Interrupted task: `main`; execution was in the USB host ISR. This is not
  evidence that ordinary main-loop code caused the failure.
- Subsequent boot reported reset code 7/watchdog and the receiver restarted.
  The preserved panic record is stronger evidence of the preceding failure
  than that reset label alone.

The channel interrupt bits were optimized out. Channel/pipe heap objects were
not retained in this dump, so the error subtype, endpoint and electrical or
scheduling trigger cannot be recovered from it. Root cause beyond the failing
HAL assertion is not yet proven.

## Upstream match

Espressif [issue 19111](https://github.com/espressif/esp-idf/issues/19111)
documents the same assertion on ESP32-P4 rev1.3. The associated fix explains
that a pipe-error interrupt may precede completion of channel halt; assuming
the halt bit is already present can abort a recoverable error.
[Official commit 63b468539f86f759ae1075ed277b2da65eccbdfe](https://github.com/espressif/esp-idf/commit/63b468539f86f759ae1075ed277b2da65eccbdfe)
removes that assertion, leaving error classification in place. The related
[99dafa3 backport](https://github.com/espressif/esp-idf/commit/99dafa3ad7a170e496847c805b14faf7606890f9)
contains the same change.

Both locally installed SDK copies, 5.5.4 and 6.1, still contain the assertion
at inspection. Simply selecting that installed 6.1 tree does not establish a
fix. No SDK modification or new firmware flash was made during this diagnosis.
Do not disable assertions globally or claim the underlying USB error is fixed.

## Reproduction and next gate

1. Preserve the current dump and matching ELF before a replacement flash.
2. Record the precise last UI/serial action, tuned frequency, demodulation,
   converter mode, USB topology/power and antenna.
3. Prepare an isolated build carrying the reviewed upstream HAL correction;
   do not modify a shared SDK checkout or unrelated dirty SDK files.
4. Repeat the same transition and verify continued/recovered IQ delivery,
   sensible USB error reporting, no panic and FM/RDS reception afterward.
5. Exercise USB error/recovery and existing receiver regressions before
   declaring this suitable for release. A non-recurrence is bounded evidence,
   not proof that all USB error handling is correct.

## Private artifacts and decoder procedure

Saved under the application's ignored
`build-native-hosted3/e4000-crash-20261031/`: raw 256 KiB coredump partition
read from 0x610000, matching ELF, decoded core ELF and full debugger output.
These can contain memory contents and must not be committed or published.
Only sanitized cause, stack and hashes are retained in this repository.

The installed `esp_coredump` 1.16.0 verified and decoded the panic but its
Windows GDB integration did not produce the full stack. Direct batch GDB
17.1 against the extracted core ELF recovered it. Use forward-slash paths
inside GDB `-ex` commands; unescaped Windows backslashes were consumed as
command escapes. No device-memory writes were used for decoding.
