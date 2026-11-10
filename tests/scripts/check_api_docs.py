#!/usr/bin/env python3
"""Check declared API-document versions and public-symbol coverage, not prose accuracy."""
import argparse
import re
from pathlib import Path


def public_symbols(header):
    header = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.S)
    functions = set(re.findall(r"\b(esp_rtl_sdr_\w+)\s*\([^;{}]*\)\s*;", header))
    types = set(re.findall(r"}\s*(esp_rtl_sdr_\w+_t)\s*;", header))
    return functions | types


def check(root):
    header = (root / "include/esp_rtl_sdr.h").read_text(encoding="utf-8")
    parts = [re.search(rf"^#define ESP_RTL_SDR_VERSION_{part}\s+(\d+)\s*$",
                       header, re.M).group(1) for part in ("MAJOR", "MINOR", "PATCH")]
    version = ".".join(parts)
    if re.search(r"^#define ESP_RTL_SDR_VERSION_IS_PRERELEASE\s+1\s*$", header, re.M):
        token = re.search(r"^#define ESP_RTL_SDR_VERSION_PRERELEASE\s+(\S+)", header, re.M).group(1)
        version += "-" + token.strip('"')
    errors = []
    for file, pattern in (
        ("docs/API.md", r"^\*\*Version:\*\* ([^\s]+)\s*$"),
        ("docs/API_REFERENCE.md", r"^> \*\*Version tracked:\*\* `([^`]+)`"),
    ):
        doc = (root / file).read_text(encoding="utf-8")
        declarations = re.findall(pattern, doc, re.M)
        if declarations != [version]:
            errors.append(f"{file}: declared version {declarations!r}; expected {version}")
    reference = (root / "docs/API_REFERENCE.md").read_text(encoding="utf-8")
    # An index entry alone does not count as reference coverage.
    reference = re.sub(r"^## 19\. Symbol index\n.*?(?=^## |\Z)", "", reference, flags=re.M | re.S)
    symbols = public_symbols(header)
    symbols |= public_symbols((root / "include/esp_rtl_sdr_acquisition.h").read_text(encoding="utf-8"))
    missing = symbols - set(re.findall(r"\besp_rtl_sdr_\w+\b", reference))
    errors.extend(f"docs/API_REFERENCE.md: missing public symbol {name}" for name in sorted(missing))
    return errors, version, len(symbols)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    errors, version, count = check(args.root)
    if errors:
        print("\n".join(errors))
        return 1
    print(f"API_DOCS_OK ver={version} symbols={count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
