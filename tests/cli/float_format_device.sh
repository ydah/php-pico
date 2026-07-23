#!/bin/sh
set -eu

binary=${1:-build/host/php-pico-rp2040}

actual=$(
    "$binary" -r \
        "echo sprintf('%08.2f|%-8.2e|%.3g', -1.375, 12.5, 0.0000125), ':'; var_dump(NAN, INF, -0.0); echo json_encode([1.25, INF, -0.0]);"
)
expected='-0001.38|1.25e+1 |1.25e-5:float(nan)
float(inf)
float(-0)
[1.25,null,-0]'

test "$actual" = "$expected"

paths=$(
    "$binary" -r \
        "\$tiny = 1.0 / 100000.0; echo sprintf('%.9g', \$tiny), ':', \$tiny, ':'; var_dump(\$tiny); echo str_replace(\"\\n\", '|', print_r([\$tiny], true)), ':', json_encode(\$tiny), ':', sprintf('[%08f][%08f][%08f]', INF, -INF, NAN);"
)
expected_paths='9.99999975e-6:9.9999997473788e-06:float(9.9999997473788e-06)
Array|(|    [0] => 9.9999997473788e-06|)|:9.99999975e-06:[     inf][    -inf][     nan]'

test "$paths" = "$expected_paths"

adc_voltage=$("$binary" -r "echo (new ADC(7))->read_voltage();")
test "$adc_voltage" = '0.090586848556995'

power=$(
    "$binary" -r \
        "echo pow(2, 5), ':', pow(-2, 3), ':', pow(-2, 4), ':', pow(4, -2), ':'; var_dump(pow(-2, 0.5), pow(0, -3), pow(-0.0, 3), pow(-0.0, -3), pow(2, 128), pow(0.5, INF), pow(2, -INF));"
)
expected_power='32:-8:16:0.0625:float(nan)
float(inf)
float(-0)
float(-inf)
float(inf)
float(0)
float(0)'
test "$power" = "$expected_power"

integers=$(
    "$binary" -r \
        "echo sprintf('%08d|%u|%08x|%o', -42, -1, 255, 8), ':'; print_r([-2147483648 => 2147483647]); echo ':', json_encode([-2147483648 => 2147483647]);"
)
expected_integers='-0000042|4294967295|000000ff|10:Array
(
    [-2147483648] => 2147483647
)
:{"-2147483648":2147483647}'
test "$integers" = "$expected_integers"
