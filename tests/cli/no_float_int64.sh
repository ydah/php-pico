#!/bin/sh
set -eu

binary=${1:?64-bit integer-only binary required}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-no-float-int64.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

output=$("$binary" -r \
    'echo PHP_INT_SIZE, ":", 2147483647 + 1, ":", 50000 * 50000, ":", json_decode("2147483648");')
test "$output" = '8:2147483648:2500000000:2147483648'

if "$binary" -r 'echo PHP_INT_MAX + 1;' > "$temporary/out" \
    2> "$temporary/err"; then
    echo '64-bit integer overflow unexpectedly succeeded' >&2
    exit 1
fi
test ! -s "$temporary/out"
grep -q 'integer overflow requires float support' "$temporary/err"

output=$("$binary" -r \
    '$minimum = -PHP_INT_MAX - 1; echo $minimum, ":", $minimum >> 63;')
test "$output" = '-9223372036854775808:-1'
