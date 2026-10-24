# RTL tuner-family expansion roadmap

## Direction

`esp-rtl-sdr` is intended to become a broadly compatible RTL2832U-class SDR
driver, not a driver tied to the small set of dongles available to the maintainer.
The shared RTL USB/demodulator/streaming path should remain independent from
tuner-family programming and board-specific RF routing.

The target is **broad implementation coverage with honest evidence labels**:
a profile can be implemented and available for experimental use before the
maintainer has physical hardware, but it must not be described as hardware-
verified until real reception and lifecycle tests have been run on that hardware.

## Safety rules

1. Preserve the current Blog V4, Blog V4L, Blog V3/V3c and Nooelec SMArt v5
   programming paths. Changes to shared behavior require regression tests.
2. Do not route a new tuner through an existing family's register sequence.
3. Separate tuner silicon from board properties: crystal, RF switches, GPIO,
   filters, HF upconverter, bias tee and power requirements are not implied by
   the tuner chip alone.
4. Unknown hardware remains unsupported unless a known USB identity or a
   successful, non-destructive tuner identification probe establishes a safe
   profile.
5. Keep public capability flags conservative. An implemented tuner does not
   automatically imply gain control, AGC, bandwidth, direct sampling, HF
   conversion or bias-tee support.
6. Keep the public API additive where possible. OrcSDR should continue pinning
   an explicit tested `esp-rtl-sdr` release; an application upgrade must not
   silently pull a new driver branch or floating dependency.
7. Use clean-room implementation from public datasheets and independently
   captured behavior. Reference implementations may inform expected behavior,
   but their source must not be copied into this AGPL component.

## Family rollout

| Family / board class | Plan | Evidence gate |
|---|---|---|
| R820T / R820T2 / R860 | Preserve and consolidate the current profile-specific implementation | Existing hardware coverage remains the regression baseline |
| R828D / R828S | Keep separate board profiles and RF-routing policy | Existing V4/V4L behavior must remain unchanged |
| E4000 | Experimental implementation, initially targeting Nooelec SMArTee XTR v5 | Host math/profile tests and ESP-IDF build first; physical RF acceptance pending |
| FC0012 | Add tuner-specific identification, PLL and RF path | Public datasheet-derived host tests; hardware validation later |
| FC0013 | Add tuner-specific identification, PLL and RF path | Public datasheet-derived host tests; hardware validation later |
| FC2580 | Add tuner-specific identification, PLL and RF path | Public datasheet-derived host tests; hardware validation later |
| Other RTL2832U tuners | Expand the compatibility database from known USB IDs and silicon probes | Each profile must declare unsupported features explicitly |

## E4000 branch boundary

The initial E4000 implementation is intentionally narrow:

- Recognize the Nooelec SMArTee XTR v5 descriptor or a confirmed E4000 chip ID.
- Use the E4000's own register map and fractional-N PLL plan.
- Reuse the existing RTL2832U USB and IQ streaming engine.
- Limit RF requests to the published 64 MHz–1.7 GHz tuner range.
- Do not expose tuner gain, tuner AGC, tuner bandwidth, HF upconverter,
  direct-sampling or bias-tee capabilities until implemented and justified.
- Treat the 28.8 MHz tuner reference as an experimental board assumption.
- Do not alter the current R820-family transfer tables or profile selection
  rules for already-known dongles.

## Release discipline

A new tuner profile is a public capability addition and should ship in a new
versioned driver release, not by changing the source behind an existing pinned
OrcSDR dependency. The current plan is to target the next minor pre-1.0 driver
release after CI and review pass. OrcSDR can then pin that exact release and
enable the experimental profile without changing the version used by existing
users.
