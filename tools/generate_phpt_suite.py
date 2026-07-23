#!/usr/bin/env python3
"""Generate and verify php-pico's deterministic PHPT conformance matrix."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.dont_write_bytecode = True

from phpt_suite import Case
from phpt_suite import callable_cases
from phpt_suite import class_cases
from phpt_suite import collection_cases
from phpt_suite import exception_cases
from phpt_suite import scalar_cases
from phpt_suite import stdlib_cases
from phpt_suite import string_cases


PROJECT_ROOT = Path(__file__).resolve().parent.parent
SUITE_ROOT = PROJECT_ROOT / "tests" / "phpt"
GENERATED_ROOT = SUITE_ROOT / "generated"
GENERATED_CASE_COUNT = 792
MINIMUM_SUITE_COUNT = 800
EXPECTED_FINALLY_CASE_COUNT = 18


def generated_cases() -> list[Case]:
    cases = (
        scalar_cases.cases()
        + collection_cases.cases()
        + string_cases.cases()
        + callable_cases.cases()
        + class_cases.cases()
        + stdlib_cases.cases()
        + exception_cases.cases()
    )
    paths = [case.relative_path for case in cases]
    programs = [(case.source, case.expected) for case in cases]
    if len(cases) != GENERATED_CASE_COUNT:
        raise ValueError(
            f"generator defines {len(cases)} cases, expected {GENERATED_CASE_COUNT}"
        )
    if len(paths) != len(set(paths)):
        raise ValueError("generator defines duplicate PHPT paths")
    if len(programs) != len(set(programs)):
        raise ValueError("generator defines duplicate source/expectation pairs")
    finally_count = sum(case.family == "072_finally" for case in cases)
    if finally_count != EXPECTED_FINALLY_CASE_COUNT:
        raise ValueError(
            f"generator defines {finally_count} explicit finally cases, "
            f"expected {EXPECTED_FINALLY_CASE_COUNT}"
        )
    return cases


def expected_outputs(cases: list[Case]) -> dict[Path, str]:
    return {
        GENERATED_ROOT / case.relative_path: case.render()
        for case in cases
    }


def suite_count() -> int:
    return sum(1 for _ in SUITE_ROOT.rglob("*.phpt"))


def generate(outputs: dict[Path, str]) -> None:
    for path, content in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    expected_paths = set(outputs)
    for path in GENERATED_ROOT.rglob("*.phpt"):
        if path not in expected_paths:
            path.unlink()


def verify(outputs: dict[Path, str], minimum: int) -> list[str]:
    problems = []
    expected_paths = set(outputs)
    actual_paths = set(GENERATED_ROOT.rglob("*.phpt"))
    for path in sorted(expected_paths - actual_paths):
        problems.append(f"missing generated test: {path.relative_to(PROJECT_ROOT)}")
    for path in sorted(actual_paths - expected_paths):
        problems.append(f"unexpected generated test: {path.relative_to(PROJECT_ROOT)}")
    for path in sorted(expected_paths & actual_paths):
        if path.read_text(encoding="utf-8") != outputs[path]:
            problems.append(f"stale generated test: {path.relative_to(PROJECT_ROOT)}")
    count = suite_count()
    if count < minimum:
        problems.append(f"PHPT suite has {count} tests, requires at least {minimum}")
    return problems


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check", action="store_true",
        help="verify generated files without changing them",
    )
    parser.add_argument("--minimum", type=int, default=MINIMUM_SUITE_COUNT)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        cases = generated_cases()
        outputs = expected_outputs(cases)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 2
    if not args.check:
        generate(outputs)
    problems = verify(outputs, args.minimum)
    if problems:
        for problem in problems:
            print(problem, file=sys.stderr)
        return 1
    print(
        f"PHPT suite verified: {suite_count()} tests "
        f"({len(outputs)} generated, "
        f"{sum(case.family == '072_finally' for case in cases)} explicit finally cases)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
