#!/bin/sh
set -eu

enabled=$1
disabled=$2

output=$(printf 'rccheck\nexit\n' | "$enabled" --shell)
case "$output" in
    "rccheck: OK checked="*) ;;
    *)
        echo "unexpected rccheck output: $output" >&2
        exit 1
        ;;
esac

if strings "$disabled" | grep -q 'rccheck:'; then
    echo "rccheck strings leaked into PPHP_RC_DEBUG=0 binary" >&2
    exit 1
fi
