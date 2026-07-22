#!/bin/sh
set -eu

binary=${1:?integer-only binary required}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-no-float-overflow.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

assert_overflow() {
    label=$1
    source=$2
    if "$binary" -r "$source" > "$temporary/$label.out" \
        2> "$temporary/$label.err"; then
        echo "$label unexpectedly succeeded" >&2
        exit 1
    fi
    test ! -s "$temporary/$label.out"
    grep -q 'integer overflow requires float support' "$temporary/$label.err"
    if grep -q 'runtime error:' "$temporary/$label.err"; then
        echo "$label triggered undefined-behavior sanitizer" >&2
        cat "$temporary/$label.err" >&2
        exit 1
    fi
}

minimum='(-2147483647 - 1)'
assert_overflow add 'echo 2147483647 + 1;'
assert_overflow subtract "echo $minimum - 1;"
assert_overflow multiply 'echo 50000 * 50000;'
assert_overflow divide "echo $minimum / -1;"
assert_overflow modulo "echo $minimum % -1;"
assert_overflow negate "\$value = $minimum; echo -\$value;"
assert_overflow power 'echo 2 ** 31;'
assert_overflow positive_string 'echo "2147483648" + 0;'
assert_overflow negative_string 'echo "-2147483649" + 0;'
assert_overflow sum 'echo array_sum([2147483647, 1]);'
assert_overflow product 'echo array_product([50000, 50000]);'
assert_overflow absolute "echo abs($minimum);"

assert_literal_rejected() {
    label=$1
    source=$2
    pattern=$3
    if "$binary" -r "$source" > "$temporary/$label.out" \
        2> "$temporary/$label.err"; then
        echo "$label unexpectedly succeeded" >&2
        exit 1
    fi
    test ! -s "$temporary/$label.out"
    grep -q "$pattern" "$temporary/$label.err"
}

assert_literal_rejected decimal_literal 'echo 2147483648;' \
    'integer literal requires float support'
assert_literal_rejected hex_literal 'echo 0x80000000;' \
    'integer literal requires float support'
assert_literal_rejected uint64_literal 'echo 18446744073709551615;' \
    'integer literal requires float support'
assert_literal_rejected uint64_overflow_literal 'echo 18446744073709551616;' \
    'integer literal is out of range'
assert_literal_rejected binary_overflow_literal \
    'echo 0b10000000000000000000000000000000000000000000000000000000000000000;' \
    'integer literal is out of range'
assert_literal_rejected octal_overflow_literal \
    'echo 0o2000000000000000000000;' 'integer literal is out of range'

output=$("$binary" -r \
    'echo -8 >> 1, ":", -8 >> 33, ":", 1 << 31, ":", 1 << -1;')
test "$output" = '-4:-4:-2147483648:-2147483648'
