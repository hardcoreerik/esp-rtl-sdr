# Sync status — exp/multi-dongle-0.8.0-rc1

Local box tip: complete (`e984cbb` + working tree). Host tests green:
- policy: 373 passed, 0 failed
- profiles: 76 passed, 0 failed
- TRUTH_HYGIENE_OK ver=0.8.0-rc1

Remote branch partially updated via GitHub MCP `push_files` (gh not authed on box;
`git push` HTTPS cannot prompt for credentials).

## On remote (synced)
- private/rtl_profile.hpp, rtl_control.hpp, transfers_blog_v3.hpp
- docs/PROFILES.md, SCOPE.md, CAPABILITY_MATRIX.md, AI_DEVELOPMENT_DISCLOSURE.md, API.md (version)
- tests/host/CMakeLists.txt, test_profiles.cpp, check_truth_hygiene.sh
- src/esp_rtl_sdr_policy.cpp
- idf_component.yml, library.json, examples CMakeLists

## Still need MCP/git push from local
- include/esp_rtl_sdr.h
- src/esp_rtl_sdr.cpp
- private/transfers_blog_v4.hpp (include rtl_control.hpp only)
- tests/host/test_policy.cpp
- CHANGELOG.md, PROJECT_TRUTH.md, README.md
- docs/API_REFERENCE.md
- examples/p4_serial_smoke/main/main.cpp

Patches under `/tmp/esp-rtl-sdr-patches/` on the box.
