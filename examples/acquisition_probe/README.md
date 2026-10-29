# Generic acquisition probe

A C-only driver consumer and minimal build probe. It has no display, network,
decoder or board-power dependency. The board/application must already provide
USB host VBUS. Do not flash a board without checking its power path, target,
silicon revision and recovery firmware.

Build with the installed IDF environment and an explicit supported target:

    idf.py --preview -C examples/acquisition_probe -B build/probe-s31 -D IDF_TARGET=esp32s31 build

Build success does not establish hardware support. See the portability report.
The example reuses this repository's stable esp_rtl_sdr wrapper. It polls one
receiver and prints exact span metadata; no data is retained after the call.
A failed final teardown is reported as an error; no clean state is implied.
