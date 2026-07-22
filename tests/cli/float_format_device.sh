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
