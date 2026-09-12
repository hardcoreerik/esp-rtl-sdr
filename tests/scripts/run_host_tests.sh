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
ctest --test-dir "$BUILD" -C Debug --output-on-failure
echo "HOST_TESTS_OK"

if [[ "${1:-}" == "--with-hygiene" ]]; then
  "$ROOT/tests/scripts/check_truth_hygiene.sh"
fi
