#!/usr/bin/env bash
# Version / truth hygiene — runnable locally and in CI.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

hdr="include/esp_rtl_sdr.h"
MAJOR=$(grep -E '#define ESP_RTL_SDR_VERSION_MAJOR' "$hdr" | head -1 | grep -oE '[0-9]+$')
MINOR=$(grep -E '#define ESP_RTL_SDR_VERSION_MINOR' "$hdr" | head -1 | grep -oE '[0-9]+$')
PATCH=$(grep -E '#define ESP_RTL_SDR_VERSION_PATCH' "$hdr" | head -1 | grep -oE '[0-9]+$')
VER="${MAJOR}.${MINOR}.${PATCH}"
echo "header version=$VER"

grep -q "version: \"${VER}\"" idf_component.yml
grep -q "\"version\": \"${VER}\"" library.json
grep -q "${VER}" README.md

# PROJECT_TRUTH should mention same major.minor at least
grep -qE "${MAJOR}\\.${MINOR}" PROJECT_TRUTH.md

# Required honesty docs
for f in \
  PROJECT_TRUTH.md \
  SECURITY.md \
  CONTRIBUTING.md \
  docs/AI_DEVELOPMENT_DISCLOSURE.md \
  docs/DOCUMENTATION_STANDARD.md \
  docs/TESTING_GUIDE.md \
  docs/LAB_HOBBYIST.md \
  docs/CLEAN_ROOM.md \
  docs/API.md \
  docs/API_REFERENCE.md
do
  test -f "$f" || { echo "missing $f"; exit 1; }
done

# API reference must track header major.minor (detailed ref, not marketing)
grep -qE "${MAJOR}\\.${MINOR}" docs/API_REFERENCE.md || {
  echo "docs/API_REFERENCE.md missing version ${MAJOR}.${MINOR}"
  exit 1
}
grep -qE "${MAJOR}\\.${MINOR}" docs/API.md || {
  echo "docs/API.md missing version ${MAJOR}.${MINOR}"
  exit 1
}

# CAP_GAIN / CAP_BIAS must not be advertised as on in get_capabilities source
if grep -n 'ESP_RTL_SDR_CAP_GAIN' src/esp_rtl_sdr_policy.cpp | grep -v '//' ; then
  # If GAIN appears in return bitmask, fail — should stay off until measured
  if grep -E 'return .*ESP_RTL_SDR_CAP_GAIN' src/esp_rtl_sdr_policy.cpp; then
    echo "CAP_GAIN must not be enabled in get_capabilities until measured"
    exit 1
  fi
fi
if grep -E 'return .*ESP_RTL_SDR_CAP_BIAS_TEE' src/esp_rtl_sdr_policy.cpp; then
  echo "CAP_BIAS_TEE must not be enabled until measured"
  exit 1
fi

# Host test sources present
test -f tests/host/test_policy.cpp
test -f src/esp_rtl_sdr_policy.cpp

echo "TRUTH_HYGIENE_OK ver=$VER"
