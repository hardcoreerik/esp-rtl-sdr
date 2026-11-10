"""Negative cases for documentation gates; fixtures stay outside the checkout."""
import tempfile
import unittest
from pathlib import Path
from check_api_docs import check


class DocumentationGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "include").mkdir()
        (self.root / "docs").mkdir()
        self.write("include/esp_rtl_sdr.h", """#define ESP_RTL_SDR_VERSION_MAJOR 0
#define ESP_RTL_SDR_VERSION_MINOR 9
#define ESP_RTL_SDR_VERSION_PATCH 6
#define ESP_RTL_SDR_VERSION_IS_PRERELEASE 0
/* ignored_fake_call(); */
const char *esp_rtl_sdr_get_version_string(void);
typedef struct { int value; } esp_rtl_sdr_example_t;
""")
        self.write("include/esp_rtl_sdr_acquisition.h", "void esp_rtl_sdr_sample_meta_default(void *out);")
        self.write("docs/API.md", "**Version:** 0.9.6\n")
        self.reference = "> **Version tracked:** `0.9.6`\nesp_rtl_sdr_get_version_string esp_rtl_sdr_example_t esp_rtl_sdr_sample_meta_default\n"
        self.write("docs/API_REFERENCE.md", self.reference)

    def write(self, file, text):
        (self.root / file).write_text(text, encoding="utf-8")

    def test_matching_versions_and_both_headers(self):
        self.assertEqual(check(self.root), ([], "0.9.6", 3))

    def test_stale_patch_not_hidden_by_current_version_elsewhere(self):
        for file, text in (("docs/API.md", "**Version:** 0.9.3\nHistory: 0.9.6\n"),
                           ("docs/API_REFERENCE.md", self.reference.replace("`0.9.6`", "`0.9.3`") + "History: 0.9.6\n")):
            with self.subTest(file=file):
                original = (self.root / file).read_text(encoding="utf-8")
                self.write(file, text)
                self.assertTrue(any("declared version" in e for e in check(self.root)[0]))
                self.write(file, original)

    def test_index_alone_is_not_coverage(self):
        self.write("docs/API_REFERENCE.md", self.reference.replace("esp_rtl_sdr_sample_meta_default", "") +
                   "## 19. Symbol index\nesp_rtl_sdr_sample_meta_default\n## Related documents\n")
        self.assertTrue(any("missing public symbol esp_rtl_sdr_sample_meta_default" in e for e in check(self.root)[0]))

    def test_missing_or_duplicate_version(self):
        for text in ("History 0.9.6\n", "**Version:** 0.9.6\n**Version:** 0.9.6\n"):
            self.write("docs/API.md", text)
            self.assertTrue(check(self.root)[0])

    def test_prerelease_version(self):
        header = self.root / "include/esp_rtl_sdr.h"
        self.write("include/esp_rtl_sdr.h", header.read_text(encoding="utf-8").replace(
            "IS_PRERELEASE 0", "IS_PRERELEASE 1") + '#define ESP_RTL_SDR_VERSION_PRERELEASE rc1\n')
        self.write("docs/API.md", "**Version:** 0.9.6-rc1\n")
        self.write("docs/API_REFERENCE.md", self.reference.replace("`0.9.6`", "`0.9.6-rc1`"))
        self.assertFalse(check(self.root)[0])


if __name__ == "__main__":
    unittest.main()
