#!/bin/sh
set -eu
exec python3 "$(dirname "$0")/phpt_runner.py" run "$@"
