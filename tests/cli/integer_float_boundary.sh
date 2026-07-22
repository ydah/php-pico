#!/bin/sh
set -eu

binary=${1:?RP2040-compatible UBSan binary required}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-rp-integer.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

"$binary" -r \
    '$minimum = -2147483648; echo gettype(2147483647 + 1), ":", gettype(-2147483647 - 2), ":", gettype(50000 * 50000), ":", gettype(-$minimum), ":", gettype(2147483646 + 1), ":", gettype("2147483648" + 0), ":", gettype(array_sum([2147483647])), ":", gettype(array_sum([2147483647, 1])), ":", intdiv(2147483647, 1);' \
    > "$temporary/out" 2> "$temporary/err"

test "$(cat "$temporary/out")" = \
    'double:double:double:double:integer:double:integer:double:2147483647'

if grep -q 'runtime error:' "$temporary/err"; then
    echo 'integer boundary operation triggered undefined-behavior sanitizer' >&2
    cat "$temporary/err" >&2
    exit 1
fi
test ! -s "$temporary/err"

if "$binary" -r 'echo "2147483648" % 1;' \
    > "$temporary/conversion.out" 2> "$temporary/conversion.err"; then
    echo 'out-of-range numeric string conversion unexpectedly succeeded' >&2
    exit 1
fi
test ! -s "$temporary/conversion.out"
grep -q 'integer conversion is out of range' "$temporary/conversion.err"
if grep -q 'runtime error:' "$temporary/conversion.err"; then
    echo 'numeric string conversion triggered undefined-behavior sanitizer' >&2
    cat "$temporary/conversion.err" >&2
    exit 1
fi
