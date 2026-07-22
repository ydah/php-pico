#!/bin/sh
set -eu

binary=${1:?64-bit integer-only binary required}
runtime=${2:?64-bit integer-only compiler-off binary required}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-no-float-int64.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

literal_output=$(
    "$binary" -r \
        'echo 2147483648, ":", 9223372036854775807, ":", -9223372036854775808;'
)
test "$literal_output" = '2147483648:9223372036854775807:-9223372036854775808'

printf '%s\n' \
    '<?php echo 2147483648, ":", 9223372036854775807, ":", -9223372036854775808;' \
    > "$temporary/literals.php"
"$binary" -c "$temporary/literals.php" -o "$temporary/literals.pbc"
pbc_output=$("$runtime" "$temporary/literals.pbc")
test "$pbc_output" = "$literal_output"

assert_literal_error() {
    label=$1
    source=$2
    if "$binary" -r "$source" > "$temporary/$label.out" \
        2> "$temporary/$label.err"; then
        echo "$label unexpectedly succeeded" >&2
        exit 1
    fi
    test ! -s "$temporary/$label.out"
    grep -q 'integer literal is out of range' "$temporary/$label.err"
}

assert_literal_error positive_out_of_range 'echo 9223372036854775808;'
assert_literal_error negative_out_of_range 'echo -9223372036854775809;'

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
