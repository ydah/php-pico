#!/bin/sh
set -eu

compiler=${1:?compiler-enabled binary required}
runtime=${2:?compiler-disabled binary required}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-compiler-off.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

printf '%s\n' "<?php echo 'included-';" > "$temporary/library.php"
"$compiler" -c "$temporary/library.php" -o "$temporary/library.pbc"

printf '%s\n' "<?php require '$temporary/library'; echo 'main';" \
    > "$temporary/app.php"
"$compiler" -c "$temporary/app.php" -o "$temporary/app.pbc"

output=$("$runtime" "$temporary/app.pbc")
test "$output" = "included-main"

"$runtime" -d "$temporary/app.pbc" > "$temporary/disassembly.txt"
grep -q 'INCLUDE' "$temporary/disassembly.txt"

printf '%s\n' "<?php echo 'fallback-source';" > "$temporary/fallback.php"
printf '%s\n' "<?php require '$temporary/fallback'; echo 'unreachable';" \
    > "$temporary/no_fallback.php"
"$compiler" -c "$temporary/no_fallback.php" \
    -o "$temporary/no_fallback.pbc"
if "$runtime" "$temporary/no_fallback.pbc" \
    > "$temporary/no_fallback.out" 2> "$temporary/no_fallback.err"; then
    echo "compiler-off include unexpectedly fell back to PHP source" >&2
    exit 1
fi
test ! -s "$temporary/no_fallback.out"
grep -q 'required file .* could not be opened' "$temporary/no_fallback.err"

if "$runtime" "$temporary/app.php" > /dev/null 2> "$temporary/source.err"; then
    echo "compiler-off runtime unexpectedly executed PHP source" >&2
    exit 1
fi
grep -q 'source compiler is disabled; execute PBC instead' \
    "$temporary/source.err"

if "$runtime" -r 'echo 1;' > /dev/null 2> "$temporary/inline.err"; then
    echo "compiler-off runtime unexpectedly accepted inline PHP" >&2
    exit 1
fi
grep -q 'inline PHP is unavailable because the source compiler is disabled' \
    "$temporary/inline.err"

if "$runtime" -c "$temporary/app.php" -o "$temporary/unexpected.pbc" \
    > /dev/null 2> "$temporary/compile.err"; then
    echo "compiler-off runtime unexpectedly compiled PHP source" >&2
    exit 1
fi
test ! -e "$temporary/unexpected.pbc"
grep -q 'compilation is unavailable because the source compiler is disabled' \
    "$temporary/compile.err"

if "$runtime" < /dev/null > /dev/null 2> "$temporary/repl.err"; then
    echo "compiler-off runtime unexpectedly started a REPL" >&2
    exit 1
fi
grep -q 'REPL is unavailable because the source compiler is disabled' \
    "$temporary/repl.err"
