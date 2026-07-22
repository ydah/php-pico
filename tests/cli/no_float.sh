#!/bin/sh
set -eu

float_compiler=${1:?float-enabled compiler required}
compiler=${2:?integer-only compiler required}
runtime=${3:?integer-only PBC runtime required}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-no-float.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

output=$($compiler -r \
    'echo 1 + 2, ":", 8 / 2, ":", 2 ** 5, ":", array_sum([1, 2, 3]), ":", json_encode([1, 2]);')
test "$output" = '3:4:32:6:[1,2]'

printf '%s\n' '<?php echo intdiv(9, 2), ":", abs(-7), ":", array_product([2, 3, 4]);' \
    > "$temporary/integer.php"
$compiler -c "$temporary/integer.php" -o "$temporary/integer.pbc"
output=$($runtime "$temporary/integer.pbc")
test "$output" = '4:7:24'

assert_rejected() {
    label=$1
    source=$2
    pattern=$3
    if $compiler -r "$source" > "$temporary/$label.out" 2> "$temporary/$label.err"; then
        echo "$label unexpectedly succeeded" >&2
        exit 1
    fi
    test ! -s "$temporary/$label.out"
    grep -q "$pattern" "$temporary/$label.err"
}

assert_rejected literal 'echo 1.5;' 'float support disabled'
assert_rejected exponent 'echo 1e2;' 'float support disabled'
assert_rejected cast 'echo (float) 1;' 'float support disabled'
assert_rejected math 'echo sqrt(4);' 'float support disabled'
assert_rejected floatval 'echo floatval("1");' 'float support disabled'
assert_rejected division 'echo 3 / 2;' 'non-integral division requires float support'
assert_rejected format 'echo sprintf("%f", 1);' 'invalid or incomplete format string'

output=$($compiler -r \
    'var_dump(function_exists("sqrt"), function_exists("floatval"), is_float(1));')
test "$output" = 'bool(false)
bool(false)
bool(false)'

printf '%s\n' '<?php echo 1.25;' > "$temporary/float.php"
$float_compiler -c "$temporary/float.php" -o "$temporary/float.pbc"
cp "$temporary/float.pbc" "$temporary/legacy-float.pbc"
printf '\006' | dd of="$temporary/legacy-float.pbc" bs=1 seek=6 conv=notrunc \
    2>/dev/null
test "$($float_compiler "$temporary/legacy-float.pbc")" = '1.25'
if $runtime "$temporary/legacy-float.pbc" > "$temporary/legacy.out" \
        2> "$temporary/legacy.err"; then
    echo 'integer-only runtime accepted a legacy floating-point PBC image' >&2
    exit 1
fi
test ! -s "$temporary/legacy.out"
grep -q 'PBC image requires float support' "$temporary/legacy.err"

if $runtime "$temporary/float.pbc" > "$temporary/pbc.out" 2> "$temporary/pbc.err"; then
    echo 'integer-only runtime accepted a floating-point PBC constant' >&2
    exit 1
fi
test ! -s "$temporary/pbc.out"
grep -q 'PBC image requires float support' "$temporary/pbc.err"

if nm "$compiler" | grep -E \
    'pphp_format_float|pv_float|pphp_ret_float|_(pow|fmod|floor|ceil|sqrt|sin|cos|tan|atan|exp|log|round)$' \
    > "$temporary/symbols"; then
    echo 'integer-only binary contains floating-point runtime symbols' >&2
    cat "$temporary/symbols" >&2
    exit 1
fi
