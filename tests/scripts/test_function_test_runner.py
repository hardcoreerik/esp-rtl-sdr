"""Tests for function_test_runner.py. Run: python3 -m unittest discover tests/scripts"""
import os
import unittest

import function_test_runner as ft

HERE = os.path.dirname(os.path.abspath(__file__))


def read_lines(name):
    with open(os.path.join(HERE, "fixtures", name), encoding="utf-8") as fh:
        return fh.read().splitlines()


def load(name):
    return ft.parse_lines(read_lines(name))


class RunnerTests(unittest.TestCase):
    def test_good_log_passes(self):
        run = load("function_test_good.log")
        self.assertTrue(run.complete)
        self.assertEqual(len(run.results), 3)
        code, reasons = ft.judge(run)
        self.assertEqual((code, reasons), (0, []))

    def test_failing_log_fails(self):
        code, reasons = ft.judge(load("function_test_bad.log"))
        self.assertEqual(code, 1)
        self.assertTrue(any("failed" in r for r in reasons))

    def test_requirements(self):
        run = load("function_test_good.log")
        self.assertEqual(ft.judge(run, require_devices=3)[0], 1)
        self.assertEqual(ft.judge(run, fail_on_skip=True)[0], 1)

    def test_incomplete_run_is_not_a_pass(self):
        run = ft.parse_lines(['{"ft":"begin","version":"x","sha":"y","board":"z"}'])
        self.assertEqual(ft.judge(run)[0], 2)
        self.assertEqual(ft.judge(ft.parse_lines([]))[0], 2)

    def test_lost_lines_detected(self):
        lines = read_lines("function_test_good.log")
        lines = [ln for ln in lines if "dev.identity" not in ln]
        code, reasons = ft.judge(ft.parse_lines(lines))
        self.assertEqual(code, 1)
        self.assertTrue(any("lost lines" in r for r in reasons))

    def test_new_begin_discards_partial_run(self):
        lines = ['{"ft":"begin","version":"old","sha":"1","board":"b"}',
                 '{"ft":"result","test":"stale","dev":-1,"profile":"","status":"FAIL","ms":0,"detail":""}']
        lines += read_lines("function_test_good.log")
        run = ft.parse_lines(lines)
        self.assertTrue(all(r["test"] != "stale" for r in run.results))
        self.assertEqual(ft.judge(run)[0], 0)

    def test_garbage_lines_ignored(self):
        run = ft.Run()
        self.assertFalse(ft.feed(run, '{"ft":"result", broken'))
        self.assertFalse(ft.feed(run, "random text"))

    def test_prompts_collected(self):
        run = ft.Run()
        ft.feed(run, '{"ft":"begin","version":"x","sha":"y","board":"z"}')
        ft.feed(run, "FT_PROMPT unplug dongle 0 within 30 s")
        self.assertEqual(run.prompts, ["unplug dongle 0 within 30 s"])


if __name__ == "__main__":
    unittest.main()
