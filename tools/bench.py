#!/usr/bin/env python3
"""Run the portable host side of the php-pico release benchmarks."""

from __future__ import annotations

import argparse
import subprocess
import tempfile
import time
from pathlib import Path


CASES = (
    ("fib(24) recursion", "tests/bench/fib24.php", "46368"),
    ("integer loop 1,000,000", "tests/bench/integer_loop.php", "1000000"),
    ("16-byte concatenation x10,000", "tests/bench/string_concat.php", "880"),
)


def timed(command: list[str]) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=60,
        check=False,
    )
    return completed, time.perf_counter() - started


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="build/host/php-pico")
    args = parser.parse_args()
    failed = False
    for label, source, expected in CASES:
        completed, elapsed = timed([args.binary, source])
        actual = completed.stdout.rstrip("\r\n")
        ok = completed.returncode == 0 and actual == expected
        print(f"{'PASS' if ok else 'FAIL'} {label}: {elapsed:.6f} s")
        if not ok:
            print(f"  expected {expected!r}, got {actual!r}")
            failed = True

    with tempfile.TemporaryDirectory(prefix="php-pico-bench-") as directory:
        bytecode = str(Path(directory) / "fib24.pbc")
        compiled = subprocess.run(
            [args.binary, "-c", "tests/bench/fib24.php", "-o", bytecode],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        if compiled.returncode != 0:
            print(f"FAIL PBC compile: {compiled.stdout.rstrip()}")
            failed = True
        else:
            completed, elapsed = timed([args.binary, bytecode])
            actual = completed.stdout.rstrip("\r\n")
            ok = completed.returncode == 0 and actual == "46368"
            print(f"{'PASS' if ok else 'FAIL'} PBC load and execute: {elapsed:.6f} s")
            if not ok:
                print(f"  expected '46368', got {actual!r}")
                failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
