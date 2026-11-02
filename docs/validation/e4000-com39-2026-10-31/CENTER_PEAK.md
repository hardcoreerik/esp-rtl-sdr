# E4000 receiver-centered spectrum feature

The user reports a persistent peak at the spectrum/waterfall center while
retuning, even without an obvious station. A hash-verified FT8 SNAP capture
shows the feature on the later 068efab candidate at 132.105 MHz, AM mode.
No retune or display-mode change was made to acquire the screenshot.

Three retained b7cb81a IQ captures at 100 MHz (source off, nominal -50 dBm on,
nominal -20 dBm on) contain a DC-region feature. The latter two were already
classified as inconclusive source-injection tests, not verified tones.
At 65,536 FFT points the strongest bin is DC, about -60 dBFS. With the display's
2048-point Hann window and per-window complex mean removal, averaged across
128 windows, the strongest bin within 20 kHz is -1,171.875 Hz, roughly
-55.2 to -55.6 dBFS. The median from 100 kHz to 1 MHz is about -73.6 dBFS.
Thus the near-center region remains about 18 dB above the surrounding floor.

The application already removes each FFT window's I and Q means before
windowing (`draw_spectrum` in ui/main.cpp). This is not evidence of a missing
simple mean-subtraction step. The offline calculation shares that preprocessing
but uses more windows for a stable measurement; it is not a pixel-exact replay.
Current screenshot and older IQ are separate observations, not contemporaneous
measurements. Values and screenshot hash are retained in center-peak.json.

This is consistent with the E4000 zero-IF receiver's DC/near-DC behavior, but
does not identify the exact analog offset, calibration, or phase-noise source.
Nooelec explicitly recommends offset tuning and I/Q correction for XTR/E4000
zero-IF receivers in its [technical support FAQ](https://support.nooelec.com/hc/en-us/articles/360059643693-Technical-Support-FAQs).
That describes the older candidate. Offset tuning is now implemented; see [current profile](../../E4000_EXPERIMENTAL.md) and [PC reference](../../captures/e4000_xtr_2026-10-31/offset/README.md). I/Q correction and broader acceptance remain separate.

Do not blank center bins or alter retained raw IQ to make the display look
cleaner: a real station's carrier can also be centered. A proper comparison
should acquire known-tone and source-off IQ, move the physical receiver LO
away from the desired channel, apply a verified digital frequency correction,
and confirm correct labels, tone amplitude, demodulation and raw metadata.
PC reference offset-tuning controls have since been captured (linked above). If tuner DC calibration is
changed, use first-party before/after evidence and test neighboring signals.
Keep the USB HAL panic correction and this signal-processing issue separate.

The first PNG request failed authentication during serial activity. A bounded
line reset and read-only status query succeeded; a subsequent PNG request
completed with its length/hash checks. No firmware reset or flash was used.
