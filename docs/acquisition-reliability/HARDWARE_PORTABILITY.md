# Hardware portability

The tested production path is ESP32-P4 high-speed USB host. Tab5 and Waveshare
P4 are distinct board integrations; neither display is a driver dependency.
The Waveshare built-in CH334 hub requires the DEVICE jumper route for its
downstream ports. Actual runs enumerated V4 on P1.p3 and a V3/R820T2-profile
receiver on P1.p2. That second receiver is not Nooelec evidence.

The generic C probe also compiled for ESP32-S31 using installed ESP-IDF v6.1
and managed USB host 1.6.0. This is build evidence only: no S31 hardware is
available locally. The core-affinity default accommodates single-core targets.
S31 hardware, board power/routing and sustained acquisition remain NOT TESTED.

S2/S3 full-speed USB is not a supported equivalent of this high-speed driver.
2.4 MS/s CU8 alone needs 4.8 MB/s, exceeding the full-speed 12 Mbit/s signaling
rate before overhead. A reduced-rate implementation would require separate
qualification. USB Serial/JTAG is not a general SDR host controller.

Official references, checked 2026-10-29:

- [Espressif USB overview](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_overview.html)
- [S31 preliminary datasheet](https://documentation.espressif.com/esp32-s31_datasheet_en.html)
- [IDF 6.1 S31 getting started](https://docs.espressif.com/projects/esp-idf/en/release-v6.1/esp32s31/get-started/index.html)

SDK 5.5.4 uses bundled USB host; SDK 6.1 uses conditional managed USB host 1.6.
The locally installed 5.5.4 SDK already contains unrelated modifications; they
were preserved. SDK 5.5.6 and physical PSRAM configurations were NOT RUN.
