#!/bin/sh
set -eu

binary=${1:?RP2040-compatible UBSan binary required}
int64_binary=${2:?64-bit integer and float binary required}
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

if "$binary" -r 'echo "-2147483649" % 1;' \
    > "$temporary/negative-conversion.out" \
    2> "$temporary/negative-conversion.err"; then
    echo 'negative out-of-range numeric string conversion unexpectedly succeeded' >&2
    exit 1
fi
test ! -s "$temporary/negative-conversion.out"
grep -q 'integer conversion is out of range' \
    "$temporary/negative-conversion.err"
test ! -s "$temporary/conversion.out"
grep -q 'integer conversion is out of range' "$temporary/conversion.err"
if grep -q 'runtime error:' "$temporary/conversion.err"; then
    echo 'numeric string conversion triggered undefined-behavior sanitizer' >&2
    cat "$temporary/conversion.err" >&2
    exit 1
fi

"$binary" -r \
    'var_dump(json_decode("16777217"), "16777217" + 0); echo sprintf("%d", "16777217"), ":"; var_dump(json_decode("2147483647"), "-16777217" + 0, "  +16777217 " + 0, "16777217tail" + 0, (int) "16777217", -"16777217", array_sum(["16777217"]), intdiv("16777217", 1), abs("-16777217")); echo gettype("16777217.0" + 0), ":", gettype("16777217e0" + 0), ":"; var_dump("-2147483648" + 0);' \
    > "$temporary/exact.out" 2> "$temporary/exact.err"

expected_exact='int(16777217)
int(16777217)
16777217:Warning: A non-numeric value encountered on line 1
int(2147483647)
int(-16777217)
int(16777217)
int(16777217)
int(16777217)
int(-16777217)
int(16777217)
int(16777217)
int(16777217)
double:double:int(-2147483648)'
test "$(cat "$temporary/exact.out")" = "$expected_exact"
test ! -s "$temporary/exact.err"

assert_program_modes() {
    target=$1
    label=$2
    source=$3
    expected=$4
    "$target" -r "$source" > "$temporary/$label-source.out" \
        2> "$temporary/$label-source.err"
    test "$(cat "$temporary/$label-source.out")" = "$expected"
    test ! -s "$temporary/$label-source.err"

    printf '%s\n' "<?php $source" > "$temporary/$label.php"
    "$target" "$temporary/$label.php" > "$temporary/$label-file.out" \
        2> "$temporary/$label-file.err"
    test "$(cat "$temporary/$label-file.out")" = "$expected"
    test ! -s "$temporary/$label-file.err"

    "$target" -c "$temporary/$label.php" -o "$temporary/$label.pbc"
    "$target" "$temporary/$label.pbc" > "$temporary/$label-pbc.out" \
        2> "$temporary/$label-pbc.err"
    test "$(cat "$temporary/$label-pbc.out")" = "$expected"
    test ! -s "$temporary/$label-pbc.err"
}

assert_boundaries() {
    target=$1
    label=$2
    maximum=$3
    positive_out=$4
    minimum=$5
    negative_out=$6
    expected='integer:double:integer:double|integer:double:integer:double|integer:double:integer:double'
    source="echo gettype($maximum), ':', gettype($positive_out), ':', gettype($minimum), ':', gettype($negative_out), '|'; echo gettype(\"$maximum\" + 0), ':', gettype(\"$positive_out\" + 0), ':', gettype(\"$minimum\" + 0), ':', gettype(\"$negative_out\" + 0), '|'; echo gettype(json_decode(\"$maximum\")), ':', gettype(json_decode(\"$positive_out\")), ':', gettype(json_decode(\"$minimum\")), ':', gettype(json_decode(\"$negative_out\"));"
    assert_program_modes "$target" "$label" "$source" "$expected"
}

assert_boundaries "$binary" int32 2147483647 2147483648 \
    -2147483648 -2147483649
assert_boundaries "$int64_binary" int64 9223372036854775807 \
    9223372036854775808 -9223372036854775808 -9223372036854775809

assert_base_boundaries() {
    target=$1
    label=$2
    hex_maximum=$3
    hex_out=$4
    binary_maximum=$5
    binary_out=$6
    octal_maximum=$7
    octal_out=$8
    expected='integer:double:integer:double|integer:double:integer:double|integer:double:integer:double'
    source="echo gettype($hex_maximum), ':', gettype($hex_out), ':', gettype(-$hex_out), ':', gettype(-$hex_out - 1), '|'; echo gettype($binary_maximum), ':', gettype($binary_out), ':', gettype(-$binary_out), ':', gettype(-$binary_out - 1), '|'; echo gettype($octal_maximum), ':', gettype($octal_out), ':', gettype(-$octal_out), ':', gettype(-$octal_out - 1);"
    assert_program_modes "$target" "$label" "$source" "$expected"
}

assert_base_boundaries "$binary" int32-bases 0x7fffffff 0x80000000 \
    0b1111111111111111111111111111111 \
    0b10000000000000000000000000000000 0o17777777777 0o20000000000
assert_base_boundaries "$int64_binary" int64-bases \
    0x7fffffffffffffff 0x8000000000000000 \
    0b111111111111111111111111111111111111111111111111111111111111111 \
    0b1000000000000000000000000000000000000000000000000000000000000000 \
    0o777777777777777777777 0o1000000000000000000000

huge=1
huge_digits=0
while test "$huge_digits" -lt 400; do
    huge="${huge}0"
    huge_digits=$((huge_digits + 1))
done
fallback_source="echo gettype(18446744073709551615), ':', gettype(18446744073709551616), ':', gettype(-18446744073709551616), ':', gettype(0xffffffffffffffff), ':', gettype(0x10000000000000000), ':', gettype(0b10000000000000000000000000000000000000000000000000000000000000000), ':', gettype(0o2000000000000000000000), ':', gettype($huge), ':', gettype(-$huge), '|'; echo (18446744073709551615 === \"18446744073709551615\" + 0 ? 1 : 0), ':', (18446744073709551615 === json_decode(\"18446744073709551615\") ? 1 : 0), ':', (18446744073709551616 === \"18446744073709551616\" + 0 ? 1 : 0), ':', (18446744073709551616 === json_decode(\"18446744073709551616\") ? 1 : 0), ':', (-18446744073709551616 === \"-18446744073709551616\" + 0 ? 1 : 0), ':', (-18446744073709551616 === json_decode(\"-18446744073709551616\") ? 1 : 0), ':', ($huge === \"$huge\" + 0 ? 1 : 0), ':', ($huge === json_decode(\"$huge\") ? 1 : 0);"
fallback_expected='double:double:double:double:double:double:double:double:double|1:1:1:1:1:1:1:1'
assert_program_modes "$binary" int32-fallback "$fallback_source" \
    "$fallback_expected"
assert_program_modes "$int64_binary" int64-fallback "$fallback_source" \
    "$fallback_expected"

float32_rounding_source='echo (2147483776 === 2147483648 ? 1 : 0), ":", (2147483777 === 2147483904 ? 1 : 0), ":", (2147483776 === "2147483776" + 0 ? 1 : 0), ":", (2147483777 === json_decode("2147483777") ? 1 : 0);'
assert_program_modes "$binary" int32-rne "$float32_rounding_source" '1:1:1:1'

float64_rounding_source='echo (18446744073709551615 === 18446744073709551616 ? 1 : 0), ":", (18446744073709553664 === 18446744073709551616 ? 1 : 0), ":", (18446744073709553665 === 18446744073709555712 ? 1 : 0), ":", (18446744073709553664 === "18446744073709553664" + 0 ? 1 : 0), ":", (18446744073709553665 === json_decode("18446744073709553665") ? 1 : 0);'
assert_program_modes "$int64_binary" int64-rne "$float64_rounding_source" \
    '1:1:1:1:1'

assert_trailing_prefix() {
    target=$1
    label=$2
    literal=$3
    "$target" -r \
        "echo ($literal === \"${literal}tail\" + 0 ? 1 : 0);" \
        > "$temporary/$label.out" 2> "$temporary/$label.err"
    expected="Warning: A non-numeric value encountered on line 1
1"
    test "$(cat "$temporary/$label.out")" = "$expected"
    test ! -s "$temporary/$label.err"
}

assert_trailing_prefix "$binary" int32-trailing 18446744073709551616
assert_trailing_prefix "$int64_binary" int64-trailing 18446744073709553665

assert_integer_conversion_error() {
    target=$1
    label=$2
    source=$3
    if "$target" -r "$source" > "$temporary/$label.out" \
        2> "$temporary/$label.err"; then
        echo "$label unexpectedly succeeded" >&2
        exit 1
    fi
    test ! -s "$temporary/$label.out"
    grep -q 'integer conversion is out of range' "$temporary/$label.err"
}

assert_integer_conversion_error "$binary" int32-positive-cast \
    'echo (int) "2147483648";'
assert_integer_conversion_error "$binary" int32-negative-cast \
    'echo (int) "-2147483649";'
assert_integer_conversion_error "$int64_binary" int64-positive-cast \
    'echo (int) "9223372036854775808";'
assert_integer_conversion_error "$int64_binary" int64-negative-cast \
    'echo (int) "-9223372036854775809";'

if "$int64_binary" -r 'echo round(1.0, 9223372036854775807);' \
    > "$temporary/int64-round.out" 2> "$temporary/int64-round.err"; then
    echo 'out-of-range 64-bit round precision unexpectedly succeeded' >&2
    exit 1
fi
test ! -s "$temporary/int64-round.out"
grep -q 'round() precision is out of range' "$temporary/int64-round.err"
