# Generic acquisition probe

A C-only driver consumer and minimal build probe. It has no display, network,
decoder or board-power dependency. The board/application must already provide
USB host VBUS. Do not flash a board without checking its power path, target,
silicon revision and recovery firmware.

Build with the installed IDF environment and an explicit supported target:

    idf.py --preview -C examples/acquisition_probe -B build/probe-s31 -D IDF_TARGET=esp32s31 build

Build success does not establish hardware support. See the portability report.
The example reuses this repository's stable esp_rtl_sdr wrapper. It supports one to three
receivers selected in Kconfig and prints span integrity/quality summaries.
Receiver zero is retuned, restarted and released while peers continue.
The duration is configurable; payload data is not retained after each read.
A failed final teardown is reported as an error; no clean state is implied.

For the tested Waveshare rev1.x built-in hub setup, use a fresh build directory:

    idf.py -C examples/acquisition_probe -B build/probe-waveshare -D SDKCONFIG_DEFAULTS=sdkconfig.waveshare_multi -D IDF_TARGET=esp32p4 build

The supplied profile selects two receivers and 15 minutes. Existing sdkconfig
values override defaults; inspect the resulting configuration before flashing.
