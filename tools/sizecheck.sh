#!/bin/sh
set -eu

binary=${1:-build/host/php-pico}
if [ ! -f "$binary" ]; then
    echo "sizecheck: missing binary: $binary" >&2
    exit 1
fi

mkdir -p docs
bytes=$(wc -c < "$binary" | tr -d ' ')
ram=''
rom=''
case "$binary" in
    *.elf)
        size_tool=${SIZE_TOOL:-arm-none-eabi-size}
        if command -v "$size_tool" >/dev/null 2>&1; then
            set -- $("$size_tool" "$binary" | awk 'NR == 2 { print $1, $2, $3 }')
            rom=$(($1 + $2))
            ram=$(($2 + $3))
        fi
        ;;
esac
commit=$(git rev-parse --short HEAD 2>/dev/null || printf 'working-tree')
timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

if [ ! -f docs/size.csv ]; then
    printf 'timestamp,commit,target,bytes,rom,ram\n' > docs/size.csv
fi
printf '%s,%s,%s,%s,%s,%s\n' "$timestamp" "$commit" "$binary" "$bytes" "$rom" "$ram" >> docs/size.csv
printf '%s: %s bytes\n' "$binary" "$bytes"
if [ -n "$rom" ]; then
    printf 'ROM: %s bytes, RAM: %s bytes\n' "$rom" "$ram"
    if [ "$rom" -gt "${PPHP_ROM_LIMIT:-98304}" ] ||
       [ "$ram" -gt "${PPHP_RAM_LIMIT:-143360}" ]; then
        echo 'sizecheck: RP2040 footprint exceeds configured MUST limit' >&2
        exit 1
    fi
fi
