#!/usr/bin/env python3
"""Integration tests for PHPT output and process-status handling."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
RUNNER = PROJECT_ROOT / "tools" / "phpt_runner.py"
sys.dont_write_bytecode = True
sys.path.insert(0, str(PROJECT_ROOT / "tools"))
import phpt_runner  # noqa: E402


class FakeSerial:
    def __init__(self, status: int) -> None:
        self.status = status
        self.pending = bytearray()

    def reset_input_buffer(self) -> None:
        self.pending.clear()

    def write(self, data: bytes) -> None:
        if data.startswith(b"upload "):
            self.pending.extend(b"READY\r\n")
        elif data.startswith(b"<?php"):
            self.pending.extend(b"OK\r\npico$ ")
        elif data.startswith(b"phpt "):
            nonce = data.rstrip(b"\n").rsplit(b" ", 1)[1]
            self.pending.extend(
                data.rstrip(b"\n") + b"\r\nexpected output"
                + b"\r\n@@PPHP_PHPT_EXIT:" + nonce + b":"
                + str(self.status).encode("ascii") + b"@@\r\npico$ "
            )

    def read(self, size: int) -> bytes:
        result = bytes(self.pending[:size])
        del self.pending[:size]
        return result


class PhptRunnerExitTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="php-pico-runner-test-")
        self.directory = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def executable(self, name: str, exit_code: int = 0,
                   signal: str | None = None) -> Path:
        path = self.directory / name
        ending = (
            f"os.kill(os.getpid(), signal.{signal})"
            if signal is not None else f"raise SystemExit({exit_code})"
        )
        path.write_text(
            "#!/usr/bin/env python3\n"
            "import os\nimport signal\nimport sys\n"
            "sys.stdout.write('expected output')\nsys.stdout.flush()\n"
            f"{ending}\n",
            encoding="utf-8",
        )
        path.chmod(0o755)
        return path

    def phpt(self, name: str, extra: str = "") -> Path:
        path = self.directory / name
        path.write_text(
            "--TEST--\nrunner exit behavior\n"
            "--FILE--\n<?php echo 'ignored';\n"
            "--EXPECT--\nexpected output\n"
            f"{extra}",
            encoding="utf-8",
        )
        return path

    def run_runner(self, mode: str, phpt: Path, binary: Path,
                   php: Path | None = None, report: Path | None = None
                   ) -> subprocess.CompletedProcess[str]:
        command = [
            sys.executable, str(RUNNER), mode,
            "--binary", str(binary),
        ]
        if php is not None:
            command.extend(("--php", str(php)))
        if report is not None:
            command.extend(("--report", str(report)))
        command.append(str(phpt))
        return subprocess.run(
            command, cwd=PROJECT_ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, check=False,
        )

    def test_zero_exit_is_the_default(self) -> None:
        result = self.run_runner(
            "run", self.phpt("normal.phpt"), self.executable("normal"),
        )
        self.assertEqual(0, result.returncode, result.stdout)
        self.assertIn("PASS", result.stdout)

    def test_matching_output_with_unexpected_exit_fails(self) -> None:
        result = self.run_runner(
            "run", self.phpt("exit7.phpt"), self.executable("exit7", 7),
        )
        self.assertEqual(1, result.returncode, result.stdout)
        self.assertIn("FAIL", result.stdout)
        self.assertIn("expected exit 0, actual exit 7", result.stdout)

    def test_expect_exit_allows_an_explicit_status(self) -> None:
        result = self.run_runner(
            "run", self.phpt("expected7.phpt", "--EXPECT_EXIT--\n7\n"),
            self.executable("expected7", 7),
        )
        self.assertEqual(0, result.returncode, result.stdout)
        self.assertIn("PASS", result.stdout)

    def test_invalid_expect_exit_is_borked(self) -> None:
        result = self.run_runner(
            "run", self.phpt("invalid.phpt", "--EXPECT_EXIT--\nseven\n"),
            self.executable("invalid"),
        )
        self.assertEqual(1, result.returncode, result.stdout)
        self.assertIn("BORK", result.stdout)
        self.assertIn("must contain one decimal integer", result.stdout)

    def test_invalid_expect_exit_is_borked_in_diff_mode(self) -> None:
        executable = self.executable("invalid-diff")
        result = self.run_runner(
            "diff", self.phpt("invalid-diff.phpt", "--EXPECT_EXIT--\n7x\n"),
            executable, executable, self.directory / "invalid-diff.html",
        )
        self.assertEqual(1, result.returncode, result.stdout)
        self.assertIn("BORK", result.stdout)

    def test_signal_is_not_accepted_as_success(self) -> None:
        result = self.run_runner(
            "run", self.phpt("signal.phpt"),
            self.executable("signal", signal="SIGTERM"),
        )
        self.assertEqual(1, result.returncode, result.stdout)
        self.assertIn("expected exit 0, actual signal 15", result.stdout)

    def test_diff_requires_matching_exit_codes(self) -> None:
        report = self.directory / "report.html"
        result = self.run_runner(
            "diff", self.phpt("diff.phpt"), self.executable("pico", 7),
            self.executable("php"), report,
        )
        self.assertEqual(1, result.returncode, result.stdout)
        self.assertIn("FAIL", result.stdout)
        contents = report.read_text(encoding="utf-8")
        self.assertIn("PHP exit 0, php-pico exit 7", contents)
        self.assertIn("<div>exit 0</div>", contents)
        self.assertIn("<div>exit 7</div>", contents)

    def test_declared_exit_difference_is_reported(self) -> None:
        result = self.run_runner(
            "diff",
            self.phpt(
                "declared.phpt",
                "--PHP_PICO_DIFF--\nintentional exit difference\n",
            ),
            self.executable("declared-pico", 7), self.executable("declared-php"),
            self.directory / "declared.html",
        )
        self.assertEqual(0, result.returncode, result.stdout)
        self.assertIn("DIFF", result.stdout)

    def test_fake_serial_reports_zero_and_nonzero_status(self) -> None:
        for expected_status in (0, 7):
            with self.subTest(status=expected_status):
                status, output = phpt_runner.execute_serial(
                    FakeSerial(expected_status), "<?php echo 'expected output';"
                )
                self.assertEqual(expected_status, status)
                self.assertEqual("expected output", output)


if __name__ == "__main__":
    unittest.main()
