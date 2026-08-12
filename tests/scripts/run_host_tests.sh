#!/usr/bin/env bash
# Run host unit tests + optional truth hygiene (no ESP-IDF). Exit 0 = pass.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/tests/host/build"
echo "esp_rtl_sdr host tests - root=$ROOT"

if [[ "${1:-}" == "--hygiene-only" ]]; then
  exec "$ROOT/tests/scripts/check_truth_hygiene.sh"
fi

mkdir -p "$BUILD"
cd "$BUILD"
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
if [[ -x ./esp_rtl_sdr_host_tests ]]; then
  ./esp_rtl_sdr_host_tests
elif [[ -x ./Debug/esp_rtl_sdr_host_tests ]]; then
  ./Debug/esp_rtl_sdr_host_tests
else
  echo "test binary not found" >&2
  exit 1
fi
echo "HOST_TESTS_OK"

if [[ "${1:-}" == "--with-hygiene" ]]; then
  "$ROOT/tests/scripts/check_truth_hygiene.sh"
fi
