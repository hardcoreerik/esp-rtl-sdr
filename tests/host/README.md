# Host tests

Pure profile / policy / multi-device logic. No ESP-IDF, no USB, no FreeRTOS.
`stubs/` supplies the handful of IDF headers the public API pulls in.

## Running them

On this machine the ESP toolchain is a RISC-V cross-compiler only, so these
build under WSL:

```sh
wsl -e bash -lc "cd /mnt/f/Ai/ESP-RTL-SDR/esp-rtl-sdr-signal-anomaly/tests/host \
  && cmake -S . -B /tmp/hb && cmake --build /tmp/hb -j4 \
  && for t in esp_rtl_sdr_host_tests esp_rtl_sdr_profile_tests esp_rtl_sdr_multi_tests; \
     do .//tmp/hb/\$t | tail -1; done"
```

Needs `g++` and `cmake` in the WSL image (`sudo apt-get install -y cmake`).

Building by hand needs `-Istubs` as well as `-I../../include -I../../private`,
and `test_policy` / `test_multi_device` need more sources than
`esp_rtl_sdr_policy.cpp` alone - use CMake rather than guessing flags.

## Last run

2026-09-21, all three suites:

```
esp_rtl_sdr_host_tests      passed=385  failed=0
esp_rtl_sdr_profile_tests   passed=359  failed=0
esp_rtl_sdr_multi_tests     passed= 77  failed=0
```
