#!/usr/bin/env python3
"""Host-only regression for tools/retune_latency_report.py."""
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "retune_latency_report.py"
spec = importlib.util.spec_from_file_location("retune_latency_report", SCRIPT)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class RetuneLatencyTests(unittest.TestCase):
    def test_parse_and_summary(self):
        raw = (
            "I (19) esp_rtl_sdr: tune record us: 0,7700,0,8000 (skipped 2 of 4)\n"
            "D (20) esp_rtl_sdr: retune phases us: drain=15000 tune=171000 frontend=0 resume=4000 total=190000\n"
            "D (21) esp_rtl_sdr: retune phases us: drain=12000 tune=98000 frontend=0 resume=5000 total=115000\n"
        )
        phases, records = module.parse_text(raw)
        self.assertEqual(len(phases), 2)
        self.assertEqual(phases[0]["total"], 190000)
        self.assertEqual(phases[1]["drain"], 12000)
        self.assertEqual(records[0]["active_us"], 15700)
        self.assertEqual(records[0]["skipped"], 2)
        summary = module.summarize(phases, records)
        self.assertEqual(summary["phase_count"], 2)
        self.assertEqual(summary["phases"]["total"]["max"], 190000)
        self.assertEqual(summary["phases"]["total"]["p50"], 152500.0)

    def test_empty_is_not_a_zero_latency_result(self):
        phases, records = module.parse_text("no RTL logs here\n")
        self.assertEqual(phases, [])
        self.assertIsNone(module.summarize(phases, records)["phases"]["total"]["mean"])

    def test_malformed_trace_is_ignored(self):
        phases, records = module.parse_text("tune record us: 12,34 (skipped 0 of 3)\n")
        self.assertEqual(phases, [])
        self.assertEqual(records, [])


if __name__ == "__main__":
    unittest.main()
