#!/bin/sh
set -eu

binary=${1:-build/host/php-pico}
temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-cli.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

version=$($binary --version)
test "$version" = "php-pico 0.1.0-dev"

inline=$($binary -r "echo 6 * 7;")
test "$inline" = "42"

repl=$(printf '%s\n' \
    '1 + 2' \
    '$value = 4' \
    'function twice($x) { return $x * 2; }' \
    'twice($value)' \
    'class Box { public function value() { return 9; } }' \
    '(new Box())->value()' | $binary)
expected='=> int(3)
=> int(4)
=> int(8)
=> int(9)'
test "$repl" = "$expected"

printf '%s\n' '<?php function square($x) { return $x * $x; } echo square(9);' \
    > "$temporary/program.php"
$binary -c "$temporary/program.php" -o "$temporary/program.pbc"
compiled=$($binary "$temporary/program.pbc")
test "$compiled" = "81"
$binary -d "$temporary/program.pbc" > "$temporary/disassembly.txt"
grep -q 'CALL' "$temporary/disassembly.txt"
$binary --tokens "$temporary/program.php" > "$temporary/tokens.txt"
grep -q 'FUNCTION' "$temporary/tokens.txt"
$binary --ast "$temporary/program.php" > "$temporary/ast.txt"
grep -q 'FUNCTION' "$temporary/ast.txt"

shell_output=$(printf '%s\n' \
    'version' \
    "echo shell-data > $temporary/shell.txt" \
    "cat $temporary/shell.txt" \
    "cp $temporary/shell.txt $temporary/shell-copy.txt" \
    "mv $temporary/shell-copy.txt $temporary/shell-moved.txt" \
    "rm $temporary/shell-moved.txt" \
    "phpc $temporary/program.php $temporary/shell.pbc" \
    "php $temporary/shell.pbc" \
    'free' \
    'quit' | $binary --shell)
printf '%s\n' "$shell_output" | grep -q 'php-pico 0.1.0-dev'
printf '%s\n' "$shell_output" | grep -q 'shell-data'
printf '%s\n' "$shell_output" | grep -q '81'
printf '%s\n' "$shell_output" | grep -q 'total='

exit 0
