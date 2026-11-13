#!/usr/bin/env python3
"""Summarize native esp-rtl-sdr retune timing logs without touching the radio.

Run from any working directory. Both CSV and JSON plus a human-readable log are
written to that directory. No network access or external dependencies required.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import statistics
from pathlib import Path

PHASE = re.compile(
    r"retune phases us:\s*drain=(\d+)\s+tune=(\d+)\s+"
    r"frontend=(\d+)\s+resume=(\d+)\s+total=(\d+)"
)
RECORD = re.compile(r"tune record us:\s*([\d,]+)\s+\(skipped\s+(\d+)\s+of\s+(\d+)\)")
FIELDS = ("drain", "tune", "frontend", "resume", "total")


def parse_text(text: str) -> tuple[list[dict[str, int]], list[dict[str, object]]]:
    """Extract successful/instrumented phase timings; do not infer missing samples."""
    phases: list[dict[str, int]] = []
    records: list[dict[str, object]] = []
    for line in text.splitlines():
        m = PHASE.search(line)
        if m:
            phases.append({key: int(value) for key, value in zip(FIELDS, m.groups())})
        r = RECORD.search(line)
        if r:
            entries = [int(v) for v in r.group(1).split(",")]
            total = int(r.group(3))
            if len(entries) != total:
                continue
            records.append({
                "skipped": int(r.group(2)),
                "records": total,
                "record_us": entries,
                "active_us": sum(entries),
            })
    return phases, records


def percentile(values: list[int], percent: float) -> float | None:
    if not values:
        return None
    v = sorted(values)
    pos = (len(v) - 1) * percent
    lo = int(pos)
    hi = min(len(v) - 1, lo + 1)
    return round(v[lo] + (v[hi] - v[lo]) * (pos - lo), 2)


def summarize(phases: list[dict[str, int]], records: list[dict[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {
        "phase_count": len(phases),
        "record_trace_count": len(records),
        "units": "microseconds",
        "phases": {},
        "notes": [
            "Retune timing is not the same as first stable, CRC-verified post-retune IQ.",
            "Records cannot be paired to individual phase events unless the device log has event IDs.",
            "This tool reports observed logs; it does not claim universal hardware performance.",
        ],
    }
    phase_stats: dict[str, object] = {}
    for key in FIELDS:
        values = [p[key] for p in phases]
        phase_stats[key] = {
            "mean": round(statistics.mean(values), 2) if values else None,
            "p50": percentile(values, 0.50),
            "p95": percentile(values, 0.95),
            "min": min(values) if values else None,
            "max": max(values) if values else None,
        }
    result["phases"] = phase_stats
    if records:
        values = [int(r["active_us"]) for r in records]
        result["tuner_record_active_us"] = {
            "mean": round(statistics.mean(values), 2),
            "p50": percentile(values, 0.50),
            "p95": percentile(values, 0.95),
        }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Captured serial log")
    parser.add_argument("--prefix", default="fast-retune", help="Output filename prefix, no path separators")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", args.prefix) or args.prefix in (".", ".."):
        parser.error("--prefix must be a simple filename without directory separators")
    raw = args.input.read_text(encoding="utf-8", errors="replace")
    phases, records = parse_text(raw)
    result = summarize(phases, records)
    out = Path.cwd()
    (out / (args.prefix + ".json")).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    with (out / (args.prefix + ".csv")).open("w", encoding="utf-8", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(phases)
    message = (f"retune events={len(phases)} tuner-record traces={len(records)}\n"
               f"total us: {result['phases']['total']}\n"
               "No improvement claim without paired, RF-validated device captures.\n")
    (out / (args.prefix + ".log")).write_text(message, encoding="utf-8")
    print(message, end="")
    if not phases:
        print("WARNING: no retune phase timing records were found; check debug logging.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
