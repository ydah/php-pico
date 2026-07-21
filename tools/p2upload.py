#!/usr/bin/env python3
"""Upload one file to P2Sh over USB CDC or UART."""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path


def read_until(port: object, marker: bytes, timeout: float = 30.0) -> bytes:
    deadline = time.monotonic() + timeout
    received = bytearray()
    while marker not in received:
        if time.monotonic() >= deadline:
            raise TimeoutError(f"timed out waiting for {marker!r}")
        received.extend(port.read(256))
    return bytes(received)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="serial device, for example /dev/ttyACM0")
    parser.add_argument("local", type=Path)
    parser.add_argument("remote", nargs="?", default="/home/app.php")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        print("p2upload requires pyserial: python3 -m pip install pyserial",
              file=sys.stderr)
        return 2
    payload = args.local.read_bytes()
    with serial.Serial(args.port, args.baud, timeout=0.1) as port:
        port.reset_input_buffer()
        port.write(f"upload {args.remote} {len(payload)}\n".encode("utf-8"))
        read_until(port, b"READY\r\n")
        port.write(payload)
        response = read_until(port, b"pico$ ")
    if b"OK\r\n" not in response:
        sys.stderr.buffer.write(response)
        return 1
    print(f"uploaded {len(payload)} bytes to {args.remote}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
