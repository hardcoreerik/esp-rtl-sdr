# Silicon & documentation map

## RTL2832U

- Official Realtek product page exists; **full register datasheet was NDA**.
- Community / mirror copies of **RTL2832U Datasheet Rev 1.4 (~53 pp, 2010)**
  circulate (e.g. alldatasheet, radiolo, references from rtl-sdr.com “leaked”).
- Labeled historically “For V4L Confidential” in some scans.
- Track ID often cited: **JATR-2265-11 Rev. 1.4**.

### Documented blocks (DS TOC — insight, not a license to paste tables)

| Block | Role for SDR |
|---|---|
| ADC | 8-bit IF / Zero-IF |
| DDC | digital downconvert |
| Resampler | sample rate (28.8 MHz crystal ratio) |
| DAGC / RF AGC limits | digital gain / clamps |
| DC cancellation | baseband DC |
| Impulse / co-channel | DVB leftovers; mostly unused in SDR |
| USB EPA / FIFO | bulk IQ pipe |
| GPIO | bias-T and board features |
| I2C master | talks to tuner (R828D) |
| IR / EEPROM | remote; USB strings |

### Memory map (also in Linux `rtl28xxu.h`)

| Region | Base | Notes |
|---|---|---|
| DEMOD | 0x0000 | demod / SDR path |
| USB | 0x2000 | SIE, endpoints, DMA |
| SYS | 0x3000 | demod ctl, GPIO, I2C |
| IR | 0xFC00 | remote (unused here) |

USB vendor command indices: DEMOD / USB / SYS / I2C (see kernel headers).

## Cousins that help more than the RTL DS alone

| Part | Docs | Why |
|---|---|---|
| **R820T** | Public datasheet (rtl-sdr.com) | Tuner architecture |
| **R820T2** | [Register description PDF](https://www.rtl-sdr.com/wp-content/uploads/2016/12/R820T2_Register_Description.pdf) | LNA/Mixer/VGA/filter bits |
| **R828D** | No separate full public DS; close to R820T family | Blog V4 tuner; 3 RF inputs + open-drain notches |
| **RTL2831U** | Same family in kernel enum | Older USB packaging; same ecosystem |
| Linux `rtl2832` / `rtl2832_sdr` | In-tree | Clean public map of registers / SDR mode |

## Blog V4 product (not gerbers)

| Source | Content |
|---|---|
| [V4 datasheet PDF](https://www.rtl-sdr.com/wp-content/uploads/2024/12/RTLSDR_V4_Datasheet_V_1_0.pdf) | R828D, 500 kHz–1.766 GHz, **2.56 MHz stable / 3.2 with drops**, 1 ppm TCXO, HF upconverter 28.8 MHz LO, bias-T 4.5 V / 180 mA, triplexer |
| rtl-sdr.com/v4 users guide | Driver install, bias-T tools |
| **Gerbers** | **Not published** by RTL-SDR Blog for V4 |

Gerbers would explain RF routing / power — **not** sample-rate programming.
SPS is RTL2832U resampler + USB host sustainability.

## What we use vs what we refuse

| Use | Refuse |
|---|---|
| DS for block names and windows | Copying librtlsdr / rtl-sdr-blog init sequences as source |
| R820T2 reg PDF for *planned* gain decode | Claiming registers work without V4 USB capture |
| Our `transfers_blog_v4.hpp` measured EP0 | Silent “try V4 tables on unknown sticks” |

## Sample rate math (hardware)

\[
\text{ratio} = (28.8\,\text{MHz} \times 2^{22}) / f_s,\quad \text{ratio} \mathbin{\&}{=} \sim 3
\]

\[
f_{\text{exact}} = (28.8\,\text{MHz} \times 2^{22}) / \text{ratio}
\]

Windows accepted by this driver: **[225–300] kHz ∪ [900–3200] kHz**
(ecosystem practice + V4 bandwidth claims). Gap band is rejected.
