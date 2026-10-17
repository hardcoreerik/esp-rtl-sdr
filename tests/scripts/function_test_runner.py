#!/usr/bin/env python3
"""Run / parse the esp_rtl_sdr function-test firmware (examples/function_test).

The firmware prints one JSON object per line, mixed with ordinary ESP log
output:  {"ft":"begin",...}  {"ft":"result",...}  {"ft":"summary",...}

  # live: reset the board, capture one full run
  function_test_runner.py --port /dev/ttyUSB0 --json report.json --require-devices 3

  # offline: judge a saved console log
  function_test_runner.py --log console.txt

Exit code: 0 all good, 1 a test failed (or a requirement was not met),
2 no complete run captured (timeout / bad log / cannot open port).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from typing import Iterable, Optional

JSON_RE = re.compile(r'(\{"ft":.*\})\s*$')
PROMPT_RE = re.compile(r"FT_PROMPT\s+(.*)")


class Run:
    def __init__(self) -> None:
        self.begin: Optional[dict] = None
        self.results: list[dict] = []
        self.summary: Optional[dict] = None
        self.prompts: list[str] = []

    @property
    def complete(self) -> bool:
        return self.begin is not None and self.summary is not None


def feed(run: Run, line: str) -> bool:
    """Feed one console line. Returns True once the summary has been seen."""
    m = PROMPT_RE.search(line)
    if m:
        run.prompts.append(m.group(1).strip())
    m = JSON_RE.search(line)
    if not m:
        return False
    try:
        obj = json.loads(m.group(1))
    except json.JSONDecodeError:
        return False
    kind = obj.get("ft")
    if kind == "begin":
        # A new run starts: drop anything left from a previous, partial one.
        run.begin, run.results, run.summary, run.prompts = obj, [], None, []
    elif kind == "result" and run.begin is not None:
        run.results.append(obj)
    elif kind == "summary" and run.begin is not None:
        run.summary = obj
        return True
    return False


def parse_lines(lines: Iterable[str]) -> Run:
    run = Run()
    for line in lines:
        if feed(run, line):
            break
    return run


def judge(run: Run, require_devices: int = 0, fail_on_skip: bool = False) -> tuple[int, list[str]]:
    """Return (exit_code, reasons)."""
    if not run.complete:
        return 2, ["no complete run (missing begin or summary)"]
    reasons: list[str] = []
    s = run.summary
    assert s is not None
    counted = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    for r in run.results:
        counted[r.get("status", "FAIL")] = counted.get(r.get("status", "FAIL"), 0) + 1
    # The firmware's own tally must match what we saw; a mismatch means lost lines.
    for key, status in (("pass", "PASS"), ("fail", "FAIL"), ("skip", "SKIP")):
        if s.get(key) != counted[status]:
            reasons.append(f"summary says {key}={s.get(key)} but log has {counted[status]} (lost lines?)")
    if counted["FAIL"] or s.get("fail"):
        reasons.append(f"{counted['FAIL']} test(s) failed")
    if not counted["PASS"]:
        reasons.append("no test passed")
    if s.get("devices", 0) < require_devices:
        reasons.append(f"{s.get('devices', 0)} dongle(s) present, {require_devices} required")
    if fail_on_skip and counted["SKIP"]:
        reasons.append(f"{counted['SKIP']} test(s) skipped (--fail-on-skip)")
    return (1 if reasons else 0), reasons


def render(run: Run) -> str:
    out = []
    if run.begin:
        out.append(f"driver {run.begin.get('version')}  sha {run.begin.get('sha')}  {run.begin.get('board')}")
    width = max([len(r.get("test", "")) for r in run.results] + [8])
    for r in run.results:
        dev = r.get("dev", -1)
        where = f"dev{dev}" if dev >= 0 else "  - "
        prof = r.get("profile") or ""
        out.append(f"{r.get('status', '?'):4}  {where:4}  {r.get('test', ''):{width}}  "
                   f"{r.get('ms', 0):>6} ms  {prof:24} {r.get('detail', '')}")
    if run.summary:
        s = run.summary
        out.append(f"-- pass={s.get('pass')} fail={s.get('fail')} skip={s.get('skip')} "
                   f"devices={s.get('devices')}")
    return "\n".join(out)


def capture_serial(port: str, baud: int, timeout_s: float, reset: bool) -> Run:
    try:
        import serial  # type: ignore
    except ImportError:
        print("pyserial is required for --port (pip install pyserial); use --log for saved logs",
              file=sys.stderr)
        raise SystemExit(2)
    run = Run()
    try:
        ser = serial.Serial(port, baud, timeout=0.5)
    except (OSError, serial.SerialException) as exc:  # type: ignore[attr-defined]
        print(f"cannot open {port}: {exc}", file=sys.stderr)
        raise SystemExit(2)
    with ser:
        if reset:
            # ESP32 auto-reset wiring: EN on RTS, BOOT on DTR.
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.rts = False
        deadline = time.monotonic() + timeout_s
        seen_prompts = 0
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").rstrip()
            if feed(run, line):
                break
            while seen_prompts < len(run.prompts):
                print(f">>> OPERATOR: {run.prompts[seen_prompts]}", flush=True)
                seen_prompts += 1
    return run


def main(argv: Optional[list[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--port", help="serial port of the function-test board")
    src.add_argument("--log", help="saved console log ('-' for stdin)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=900.0, help="seconds to wait for a full run")
    ap.add_argument("--no-reset", action="store_true", help="do not toggle RTS/DTR before capturing")
    ap.add_argument("--require-devices", type=int, default=0, metavar="N")
    ap.add_argument("--fail-on-skip", action="store_true")
    ap.add_argument("--json", metavar="FILE", help="write the parsed run as JSON")
    args = ap.parse_args(argv)

    if args.port:
        run = capture_serial(args.port, args.baud, args.timeout, not args.no_reset)
    else:
        fh = sys.stdin if args.log == "-" else open(args.log, encoding="utf-8", errors="replace")
        with fh:
            run = parse_lines(fh)

    print(render(run))
    code, reasons = judge(run, args.require_devices, args.fail_on_skip)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as out:
            json.dump({"begin": run.begin, "results": run.results, "summary": run.summary,
                       "verdict": {"exit": code, "reasons": reasons}}, out, indent=2)
    for reason in reasons:
        print(f"FAILED: {reason}", file=sys.stderr)
    print("FUNCTION TEST " + ("OK" if code == 0 else "FAILED"))
    return code


if __name__ == "__main__":
    sys.exit(main())
