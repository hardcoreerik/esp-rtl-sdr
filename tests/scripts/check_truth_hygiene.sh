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

grep -q "version: \"${VER}\"" idf_component.yml || {
  echo "idf_component.yml missing version \"${VER}\""
  exit 1
}
grep -q "\"version\": \"${VER}\"" library.json || {
  echo "library.json missing version \"${VER}\""
  exit 1
}
grep -q "${VER}" README.md || {
  echo "README.md missing version string ${VER} (badge/docs lag header)"
  exit 1
}

# PROJECT_TRUTH should mention same major.minor at least
grep -qE "${MAJOR}\\.${MINOR}" PROJECT_TRUTH.md || {
  echo "PROJECT_TRUTH.md missing ${MAJOR}.${MINOR}"
  exit 1
}

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
  docs/API_REFERENCE.md \
  docs/SCOPE.md \
  docs/SOAK.md \
  docs/KCONFIG.md \
  docs/TROUBLESHOOTING.md \
  docs/EXAMPLES.md \
  docs/RUNTIME_CONSTANTS.md \
  docs/lab/SOAK_LOG_TEMPLATE.md \
  docs/REVIEW_GAPS_2026-08.md
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

# CAP_GAIN / CAP_BIAS may only appear in get_capabilities with MEASURED marker + tables
if grep -E 'ESP_RTL_SDR_CAP_GAIN|ESP_RTL_SDR_CAP_BIAS_TEE' src/esp_rtl_sdr_policy.cpp | grep -v '//' >/dev/null; then
  if ! grep -q 'MEASURED_2026_08_12' src/esp_rtl_sdr_policy.cpp; then
    echo "CAP_GAIN/BIAS enabled without MEASURED_2026_08_12 marker in policy"
    exit 1
  fi
  if ! test -f private/measured_gain_bias_v4.hpp; then
    echo "missing private/measured_gain_bias_v4.hpp for CAP_GAIN/BIAS"
    exit 1
  fi
fi

# Host test sources present
test -f tests/host/test_policy.cpp
test -f src/esp_rtl_sdr_policy.cpp

echo "TRUTH_HYGIENE_OK ver=$VER"
