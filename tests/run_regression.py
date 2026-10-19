#!/usr/bin/env python3
"""Run the KamikazeOS headless regression suite.

  python tests/run_regression.py                  # build, then run everything
  python tests/run_regression.py --no-build -k F2 # filter by test-id substring
  python tests/run_regression.py --elf X.elf --baseline-elf old.elf

Each run gets build/test-runs/<timestamp>/ with one folder per test holding
its disposable disk image(s), serial log, and analyzer/dashboard output, so
every result can be audited afterwards. build/recovery.img is never used.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STORE = ROOT / "build" / "recovery.img"


def sha256(path: Path) -> str | None:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None


def build_kernel() -> None:
    script = ROOT / "scripts" / "build.ps1"
    subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                    "-File", str(script)], check=True)


def iter_tests(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from iter_tests(item)
        else:
            yield item


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", type=Path, default=ROOT / "build" / "kamikaze.elf")
    ap.add_argument("--baseline-elf", type=Path)
    ap.add_argument("--run-dir", type=Path)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("-k", action="append", default=[],
                    help="only run tests whose id contains this text")
    ap.add_argument("--exclude", action="append", default=[],
                    help="exclude matching test-id substrings and record the omitted IDs")
    args = ap.parse_args()

    if not args.no_build:
        build_kernel()
    if not args.elf.exists():
        print(f"error: kernel not found: {args.elf}")
        return 2

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = args.run_dir or ROOT / "build" / "test-runs" / stamp
    run_dir.mkdir(parents=True, exist_ok=True)
    os.environ["KMZ_TEST_ELF"] = str(args.elf.resolve())
    os.environ["KMZ_TEST_RUN_DIR"] = str(run_dir.resolve())
    if args.baseline_elf:
        os.environ["KMZ_BASELINE_ELF"] = str(args.baseline_elf.resolve())

    store_before = sha256(STORE)
    sys.path.insert(0, str(ROOT / "tests"))
    loader = unittest.TestLoader()
    suite = loader.discover(str(ROOT / "tests"), pattern="test_*.py",
                            top_level_dir=str(ROOT / "tests"))
    if args.k:
        suite = unittest.TestSuite(t for t in iter_tests(suite)
                                   if any(k in t.id() for k in args.k))
    candidates = list(iter_tests(suite))
    excluded = [t.id() for t in candidates if any(k in t.id() for k in args.exclude)]
    suite = unittest.TestSuite(t for t in candidates if t.id() not in excluded)
    (run_dir / "selected-tests.txt").write_text("\n".join(t.id() for t in iter_tests(suite)) + "\n")
    (run_dir / "excluded-tests.txt").write_text("\n".join(excluded) + ("\n" if excluded else ""))

    print(f"kernel   : {args.elf}  sha256={sha256(args.elf)[:16]}")
    print(f"run dir  : {run_dir}")
    if excluded:
        print(f"explicitly excluded: {len(excluded)} (see excluded-tests.txt)")
    with (run_dir / "summary.txt").open("w") as fh:
        runner = unittest.TextTestRunner(stream=Tee(sys.stderr, fh), verbosity=2)
        result = runner.run(suite)

    store_after = sha256(STORE)
    if store_before != store_after:
        print("FATAL: build/recovery.img changed during the test run")
        return 3
    print(f"build/recovery.img untouched: {store_after is None or store_after[:16]}")
    return 0 if result.wasSuccessful() else 1


class Tee:
    def __init__(self, *streams):
        self.streams = streams

    def write(self, data):
        for s in self.streams:
            s.write(data)

    def flush(self):
        for s in self.streams:
            s.flush()


if __name__ == "__main__":
    sys.exit(main())
