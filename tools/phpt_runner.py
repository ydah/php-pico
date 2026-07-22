#!/usr/bin/env python3
"""Small PHPT runner for php-pico's documented section subset."""

from __future__ import annotations

import argparse
import html
import re
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path


SECTION = re.compile(r"^--([A-Z_]+)--\s*$", re.MULTILINE)


@dataclass
class CaseResult:
    path: Path
    status: str
    expected: str
    actual: str
    detail: str = ""


def parse_phpt(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    matches = list(SECTION.finditer(text))
    sections: dict[str, str] = {}
    for index, match in enumerate(matches):
        start = match.end()
        if start < len(text) and text[start] == "\n":
            start += 1
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        sections[match.group(1)] = text[start:end].rstrip("\n")
    return sections


def expectf_pattern(expected: str) -> re.Pattern[str]:
    pieces: list[str] = []
    position = 0
    tokens = re.compile(r"%[dsf%]")
    for match in tokens.finditer(expected):
        pieces.append(re.escape(expected[position : match.start()]))
        token = match.group(0)
        pieces.append(
            {
                "%d": r"[+-]?\d+",
                "%s": r"[^\n]*",
                "%f": r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][+-]?\d+)?",
                "%%": "%",
            }[token]
        )
        position = match.end()
    pieces.append(re.escape(expected[position:]))
    return re.compile(r"\A" + "".join(pieces) + r"\Z", re.DOTALL)


def normalized(output: str) -> str:
    return output.replace("\r\n", "\n").rstrip("\n")


def execute(command: list[str], source: str) -> tuple[int, str]:
    with tempfile.TemporaryDirectory(prefix="php-pico-phpt-") as directory:
        script = Path(directory) / "test.php"
        script.write_text(source, encoding="utf-8")
        completed = subprocess.run(
            [*command, str(script)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    return completed.returncode, normalized(completed.stdout)


def serial_read_until(port: object, marker: bytes,
                      timeout: float = 30.0) -> bytes:
    deadline = time.monotonic() + timeout
    received = bytearray()
    while marker not in received:
        if time.monotonic() >= deadline:
            raise TimeoutError(f"serial timeout waiting for {marker!r}")
        received.extend(port.read(256))
    return bytes(received)


def execute_serial(port: object, source: str) -> tuple[int, str]:
    payload = source.encode("utf-8")
    remote = "/home/.phpt.php"
    port.reset_input_buffer()
    port.write(f"upload {remote} {len(payload)}\n".encode("ascii"))
    serial_read_until(port, b"READY\r\n")
    port.write(payload)
    uploaded = serial_read_until(port, b"pico$ ")
    if b"OK\r\n" not in uploaded:
        return 2, normalized(uploaded.decode("utf-8", errors="replace"))
    port.write(f"php {remote}\n".encode("ascii"))
    response = serial_read_until(port, b"pico$ ")
    response = response.removesuffix(b"pico$ ")
    _, separator, response = response.partition(b"\r\n")
    if not separator:
        response = b""
    actual = normalized(response.decode("utf-8", errors="replace"))
    return 0, actual


def execute_pico(args: argparse.Namespace, source: str) -> tuple[int, str]:
    if args.target == "serial":
        return execute_serial(args.serial, source)
    return execute([args.binary], source)


def skipped(sections: dict[str, str], php: str) -> bool:
    source = sections.get("SKIPIF", "").strip()
    if not source:
        return False
    _, output = execute([php], source)
    return output.lower().startswith("skip")


def matches_expectation(sections: dict[str, str], actual: str) -> tuple[bool, str]:
    if "EXPECT" in sections:
        expected = normalized(sections["EXPECT"])
        return actual == expected, expected
    if "EXPECTF" in sections:
        expected = normalized(sections["EXPECTF"])
        return expectf_pattern(expected).match(actual) is not None, expected
    raise ValueError("missing --EXPECT-- or --EXPECTF--")


def discover(paths: list[str]) -> list[Path]:
    found: list[Path] = []
    for raw in paths:
        path = Path(raw)
        if path.is_dir():
            found.extend(path.rglob("*.phpt"))
        elif path.suffix == ".phpt":
            found.append(path)
    return sorted(set(found))


def run_cases(args: argparse.Namespace, paths: list[Path]) -> list[CaseResult]:
    results: list[CaseResult] = []
    for path in paths:
        try:
            sections = parse_phpt(path)
            if "FILE" not in sections:
                raise ValueError("missing --FILE--")
            if skipped(sections, args.php):
                result = CaseResult(path, "SKIP", "", "")
            else:
                returncode, actual = execute_pico(args, sections["FILE"])
                matches, expected = matches_expectation(sections, actual)
                status = "PASS" if matches else "FAIL"
                detail = "" if returncode in (0, 1, 255) else f"exit {returncode}"
                result = CaseResult(path, status, expected, actual, detail)
        except (OSError, UnicodeError, ValueError) as error:
            result = CaseResult(path, "BORK", "", "", str(error))
        results.append(result)
        print(f"{result.status:4} {path}")
        if result.status in {"FAIL", "BORK"}:
            if result.detail:
                print(f"     {result.detail}")
            if result.status == "FAIL":
                print(f"     expected: {result.expected!r}")
                print(f"     actual:   {result.actual!r}")
    return results


def diff_cases(args: argparse.Namespace, paths: list[Path]) -> list[CaseResult]:
    results: list[CaseResult] = []
    for path in paths:
        try:
            sections = parse_phpt(path)
            if "FILE" not in sections:
                raise ValueError("missing --FILE--")
            if skipped(sections, args.php):
                result = CaseResult(path, "SKIP", "", "")
            else:
                _, native = execute([args.php], sections["FILE"])
                _, pico = execute_pico(args, sections["FILE"])
                declared = bool(sections.get("PHP_PICO_DIFF", "").strip())
                if native == pico:
                    status = "PASS"
                elif declared:
                    status = "DIFF"
                else:
                    status = "FAIL"
                result = CaseResult(path, status, native, pico,
                                    sections.get("PHP_PICO_DIFF", "").strip())
        except (OSError, UnicodeError, ValueError) as error:
            result = CaseResult(path, "BORK", "", "", str(error))
        results.append(result)
        print(f"{result.status:4} {path}")
    return results


def write_report(path: Path, results: list[CaseResult]) -> None:
    rows = []
    for result in results:
        rows.append(
            "<tr class='{}'><td>{}</td><td>{}</td><td><pre>{}</pre></td>"
            "<td><pre>{}</pre></td><td>{}</td></tr>".format(
                html.escape(result.status.lower()),
                html.escape(str(result.path)),
                html.escape(result.status),
                html.escape(result.expected),
                html.escape(result.actual),
                html.escape(result.detail),
            )
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "<!doctype html><meta charset='utf-8'><title>php-pico differential report</title>"
        "<style>body{font-family:system-ui;margin:2rem}table{border-collapse:collapse;width:100%}"
        "td,th{border:1px solid #bbb;padding:.4rem;vertical-align:top}pre{white-space:pre-wrap}"
        ".pass{background:#e8f7e8}.diff{background:#fff6d7}.fail,.bork{background:#ffe5e5}</style>"
        "<h1>php-pico differential report</h1><table><thead><tr><th>Test</th>"
        "<th>Status</th><th>PHP / expected</th><th>php-pico / actual</th>"
        "<th>Declared difference</th></tr></thead><tbody>"
        + "".join(rows)
        + "</tbody></table>",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("run", "diff"))
    parser.add_argument("paths", nargs="*", default=["tests/phpt"])
    parser.add_argument("--binary", default="build/host/php-pico")
    parser.add_argument("--php", default="php")
    parser.add_argument("--report", default="build/reports/differential.html")
    parser.add_argument("--target", choices=("host", "serial"), default="host")
    parser.add_argument("--port", help="serial device for --target=serial")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_intermixed_args()
    args.serial = None
    if args.target == "serial":
        if not args.port:
            parser.error("--port is required with --target=serial")
        try:
            import serial  # type: ignore[import-not-found]
        except ImportError:
            parser.error("serial target requires pyserial")
        args.serial = serial.Serial(args.port, args.baud, timeout=0.1)
        serial_read_until(args.serial, b"pico$ ", timeout=10.0)
    paths = discover(args.paths or ["tests/phpt"])
    if not paths:
        print("no PHPT files found", file=sys.stderr)
        return 2
    try:
        results = (run_cases(args, paths) if args.mode == "run"
                   else diff_cases(args, paths))
    finally:
        if args.serial is not None:
            args.serial.close()
    if args.mode == "diff":
        write_report(Path(args.report), results)
        print(f"report: {args.report}")
    failures = sum(result.status in {"FAIL", "BORK"} for result in results)
    print(f"{len(results)} tests, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
