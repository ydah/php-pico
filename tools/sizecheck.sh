#!/bin/sh
set -eu

binary=${1:-build/host/php-pico}
if [ ! -f "$binary" ]; then
    echo "sizecheck: missing binary: $binary" >&2
    exit 1
fi

mkdir -p docs
bytes=$(wc -c < "$binary" | tr -d ' ')
commit=$(git rev-parse --short HEAD 2>/dev/null || printf 'working-tree')
timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ')

if [ ! -f docs/size.csv ]; then
    printf 'timestamp,commit,target,bytes\n' > docs/size.csv
fi
printf '%s,%s,%s,%s\n' "$timestamp" "$commit" "$binary" "$bytes" >> docs/size.csv
printf '%s: %s bytes\n' "$binary" "$bytes"

