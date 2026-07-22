#!/bin/sh
set -eu

warnings_on=${1:?warnings-on compiler binary required}
warnings_off=${2:?warnings-off compiler binary required}
pbc_on=${3:?warnings-on PBC binary required}
pbc_off=${4:?warnings-off PBC binary required}
no_float=${5:?integer-only binary required}
rp_equivalent=${6:?RP2040-equivalent binary required}
ubsan=${7:?UBSan binary required}

temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

fail() {
    echo "warnings test: $*" >&2
    exit 1
}

assert_output() {
    binary=$1
    source=$2
    expected=$3
    actual=$($binary -r "$source")
    test "$actual" = "$expected" || {
        echo "expected:" >&2
        printf '%s\n' "$expected" >&2
        echo "actual:" >&2
        printf '%s\n' "$actual" >&2
        fail "unexpected output from $binary"
    }
}

undefined_source='echo "begin\n";
echo $missing;
$missing = null;
echo $missing, "defined\n";
unset($missing);
echo $missing, "end\n";'
undefined_on='begin
Warning: Undefined variable $missing on line 2
defined
Warning: Undefined variable $missing on line 6
end'
undefined_off='begin
defined
end'
assert_output "$warnings_on" "$undefined_source" "$undefined_on"
assert_output "$warnings_off" "$undefined_source" "$undefined_off"

local_source='function inspect_vars() {
echo $local, "local\n";
$local = null;
echo $local, "defined\n";
unset($local);
echo $local, "again\n";
global $bound;
echo $bound, "global\n";
$bound = 5;
echo $bound, "set\n";
unset($bound);
echo $bound, "gone\n";
}
inspect_vars();'
local_on='Warning: Undefined variable $local on line 2
local
defined
Warning: Undefined variable $local on line 6
again
Warning: Undefined variable $bound on line 8
global
5set
Warning: Undefined variable $bound on line 12
gone'
local_off='local
defined
again
global
5set
gone'
assert_output "$warnings_on" "$local_source" "$local_on"
assert_output "$warnings_off" "$local_source" "$local_off"

quiet_source='echo (isset($quiet) ? 1 : 0), ":", (empty($quiet) ? 1 : 0), ":", ($quiet ?? 7), ":";
$quiet ??= 8;
echo $quiet, ":";
echo (isset($missingArray["key"]) ? 1 : 0), ":", ($missingArray["key"] ?? 6), ":";
function defaults($nullable = null) { echo (isset($nullable) ? 1 : 0), ":", ($nullable ?? 5), ":"; }
defaults();
defaults(null);
function static_null() { static $value; echo (isset($value) ? 1 : 0), ":", ($value ?? 3), "\n"; }
static_null();
static_null();
class QuietProbe { public $value = 5; public $child; }
$quietEffects = 0;
function quiet_name() { global $quietEffects; $quietEffects++; return "value"; }
$nullBase = null;
echo (isset($nullBase?->{quiet_name()}) ? 1 : 0), ":", (empty($nullBase?->{quiet_name()}) ? 1 : 0), ":", ($nullBase?->{quiet_name()} ?? 7), ":", ($nullBase?->value ?? 11), ":", $quietEffects, ":";
$box = new QuietProbe();
echo (isset($box?->{quiet_name()}) ? 1 : 0), ":", (empty($box?->{quiet_name()}) ? 1 : 0), ":", ($box?->{quiet_name()} ?? 0), ":", ($box?->value ?? 0), ":", $quietEffects, ":";
$outer = new QuietProbe();
$outer->child = $box;
echo ($outer?->child?->{quiet_name()} ?? 0), ":", $quietEffects, ":";
$outer->child = null;
echo ($outer?->child?->{quiet_name()} ?? 9), ":", $quietEffects, "\n";'
quiet_expected='0:1:7:8:0:6:0:5:0:5:0:3
0:3
0:1:7:11:0:1:0:5:5:3:5:4:9:4'
assert_output "$warnings_on" "$quiet_source" "$quiet_expected"
assert_output "$warnings_off" "$quiet_source" "$quiet_expected"

chain_source='class ChainProbe { public $value = 5; public $child; public function childValue() { return $this->child; } }
$chainEffects = 0;
function chain_effect($name) { global $chainEffects; $chainEffects++; return $name; }
$nullChain = null;
echo (isset($nullChain?->child[chain_effect("key")]) ? 1 : 0), ":", (empty($nullChain?->child[chain_effect("key")]) ? 1 : 0), ":", ($nullChain?->child[chain_effect("key")] ?? 7), ":";
echo (isset($nullChain?->child->{chain_effect("value")}) ? 1 : 0), ":", (empty($nullChain?->child->{chain_effect("value")}) ? 1 : 0), ":", ($nullChain?->child->{chain_effect("value")} ?? 7), ":", $chainEffects, ":";
$normalNull = $nullChain?->child[chain_effect("key")];
echo ($normalNull === null ? 1 : 0), ":", $chainEffects, ":";
$normalCallNull = $nullChain?->childValue()[chain_effect("key")];
echo ($normalCallNull === null ? 1 : 0), ":", $chainEffects, ":";
$leaf = new ChainProbe();
$chain = new ChainProbe();
$chain->child = ["first" => ["second" => $leaf]];
echo (isset($chain?->child[chain_effect("first")][chain_effect("second")]->{chain_effect("value")}) ? 1 : 0), ":", $chainEffects, ":";
echo (empty($chain?->child[chain_effect("first")][chain_effect("second")]->{chain_effect("value")}) ? 1 : 0), ":", $chainEffects, ":";
echo ($chain?->child[chain_effect("first")][chain_effect("second")]->{chain_effect("value")} ?? 0), ":", $chainEffects, ":";
$normalValue = $chain?->child[chain_effect("first")][chain_effect("second")]->{chain_effect("value")};
echo $normalValue, ":", $chainEffects, ":";
$normalCallValue = $chain?->childValue()[chain_effect("first")][chain_effect("second")]->{chain_effect("value")};
echo $normalCallValue, ":", $chainEffects, ":";
$nullableChild = new ChainProbe();
echo ($nullableChild?->child->{chain_effect("value")} ?? 8), ":", $chainEffects, "\n";'
chain_expected='0:1:7:0:1:7:0:1:0:1:0:1:3:0:6:5:9:5:12:5:15:8:16'
assert_output "$warnings_on" "$chain_source" "$chain_expected"
assert_output "$warnings_off" "$chain_source" "$chain_expected"

numeric_source='echo "12tail" + 3, ":", -"2x", ":", +"4y", ":", "1a" + "2b", "\n";'
numeric_on='Warning: A non-numeric value encountered on line 1
Warning: A non-numeric value encountered on line 1
Warning: A non-numeric value encountered on line 1
Warning: A non-numeric value encountered on line 1
Warning: A non-numeric value encountered on line 1
15:-2:4:3'
assert_output "$warnings_on" "$numeric_source" "$numeric_on"
assert_output "$warnings_off" "$numeric_source" '15:-2:4:3'

if "$warnings_on" -r 'echo "not-numeric" + 1;' \
        >"$temporary/invalid-on.out" 2>"$temporary/invalid-on.err"; then
    fail 'fully non-numeric addition unexpectedly succeeded'
fi
test "$(cat "$temporary/invalid-on.out")" = \
    'Warning: A non-numeric value encountered on line 1' ||
    fail 'fully non-numeric addition did not emit exactly one warning'
grep -q 'unsupported operand types' "$temporary/invalid-on.err" ||
    fail 'fully non-numeric addition changed the existing operand error'

if "$warnings_off" -r 'echo "not-numeric" + 1;' \
        >"$temporary/invalid-off.out" 2>"$temporary/invalid-off.err"; then
    fail 'fully non-numeric addition unexpectedly succeeded with warnings off'
fi
test ! -s "$temporary/invalid-off.out" ||
    fail 'warnings-off binary emitted warning output'
grep -q 'unsupported operand types' "$temporary/invalid-off.err" ||
    fail 'warnings-off binary changed the existing operand error'

cat >"$temporary/warnings.php" <<'PHP'
<?php
echo (isset($pbcQuiet) ? 1 : 0), ':', ($pbcQuiet ?? 4), "\n";
echo $pbcMissing, "missing\n";
$pbcMissing = null;
unset($pbcMissing);
echo $pbcMissing, "again\n";
echo "7tail" + 2, "\n";
class PbcQuietProbe { public $value = 6; }
$pbcEffects = 0;
function pbc_quiet_name() { global $pbcEffects; $pbcEffects++; return "value"; }
$pbcNull = null;
echo (isset($pbcNull?->{pbc_quiet_name()}) ? 1 : 0), ':', (empty($pbcNull?->{pbc_quiet_name()}) ? 1 : 0), ':', ($pbcNull?->{pbc_quiet_name()} ?? 7), ':', $pbcEffects, "\n";
$pbcBox = new PbcQuietProbe();
echo (isset($pbcBox?->{pbc_quiet_name()}) ? 1 : 0), ':', (empty($pbcBox?->{pbc_quiet_name()}) ? 1 : 0), ':', ($pbcBox?->{pbc_quiet_name()} ?? 0), ':', $pbcEffects, "\n";
class PbcChainProbe { public $value = 5; public $child; }
$pbcChainEffects = 0;
function pbc_chain_effect($name) { global $pbcChainEffects; $pbcChainEffects++; return $name; }
$pbcNullChain = null;
echo (isset($pbcNullChain?->child[pbc_chain_effect("key")]) ? 1 : 0), ':', (empty($pbcNullChain?->child[pbc_chain_effect("key")]) ? 1 : 0), ':', ($pbcNullChain?->child[pbc_chain_effect("key")] ?? 7), ':';
echo (isset($pbcNullChain?->child->{pbc_chain_effect("value")}) ? 1 : 0), ':', (empty($pbcNullChain?->child->{pbc_chain_effect("value")}) ? 1 : 0), ':', ($pbcNullChain?->child->{pbc_chain_effect("value")} ?? 7), ':', $pbcChainEffects, ':';
$pbcNormalNull = $pbcNullChain?->child[pbc_chain_effect("key")];
echo ($pbcNormalNull === null ? 1 : 0), ':', $pbcChainEffects, ':';
$pbcLeaf = new PbcChainProbe();
$pbcChain = new PbcChainProbe();
$pbcChain->child = ["first" => ["second" => $pbcLeaf]];
echo (isset($pbcChain?->child[pbc_chain_effect("first")][pbc_chain_effect("second")]->{pbc_chain_effect("value")}) ? 1 : 0), ':', $pbcChainEffects, ':';
echo (empty($pbcChain?->child[pbc_chain_effect("first")][pbc_chain_effect("second")]->{pbc_chain_effect("value")}) ? 1 : 0), ':', $pbcChainEffects, ':';
echo ($pbcChain?->child[pbc_chain_effect("first")][pbc_chain_effect("second")]->{pbc_chain_effect("value")} ?? 0), ':', $pbcChainEffects, ':';
$pbcNormalValue = $pbcChain?->child[pbc_chain_effect("first")][pbc_chain_effect("second")]->{pbc_chain_effect("value")};
echo $pbcNormalValue, ':', $pbcChainEffects, ':';
$pbcNullableChild = new PbcChainProbe();
echo ($pbcNullableChild?->child->{pbc_chain_effect("value")} ?? 8), ':', $pbcChainEffects, "\n";
PHP
"$warnings_on" -c "$temporary/warnings.php" -o "$temporary/warnings.pbc"
pbc_expected='0:4
Warning: Undefined variable $pbcMissing on line 3
missing
Warning: Undefined variable $pbcMissing on line 6
again
Warning: A non-numeric value encountered on line 7
9
0:1:7:0
1:0:6:3
0:1:7:0:1:7:0:1:0:1:3:0:6:5:9:5:12:8:13'
pbc_without='0:4
missing
again
9
0:1:7:0
1:0:6:3
0:1:7:0:1:7:0:1:0:1:3:0:6:5:9:5:12:8:13'
pbc_actual=$($pbc_on "$temporary/warnings.pbc")
test "$pbc_actual" = "$pbc_expected" ||
    fail 'compiler-off PBC runtime did not preserve warning behavior'
"$pbc_on" -d "$temporary/warnings.pbc" >"$temporary/warnings.disasm"
grep -q 'LOAD_LOCAL_QUIET' "$temporary/warnings.disasm" ||
    fail 'disassembler lost the quiet local-load opcode'
grep -q 'UNSET_LOCAL' "$temporary/warnings.disasm" ||
    fail 'disassembler lost the local-unset opcode'
pbc_off_actual=$($pbc_off "$temporary/warnings.pbc")
test "$pbc_off_actual" = "$pbc_without" ||
    fail 'warnings-off compiler-off PBC runtime changed semantics'

matrix_source='echo $matrixMissing, "m:"; echo "8tail" + 1, "\n";'
matrix_expected='Warning: Undefined variable $matrixMissing on line 1
m:Warning: A non-numeric value encountered on line 1
9'
assert_output "$no_float" "$matrix_source" "$matrix_expected"
assert_output "$rp_equivalent" "$matrix_source" "$matrix_expected"
assert_output "$ubsan" "$matrix_source" "$matrix_expected"

for binary in "$warnings_off" "$pbc_off"; do
    if strings "$binary" | grep -q 'Warning: '; then
        fail "warning prefix was not compiled out of $binary"
    fi
    if strings "$binary" | grep -q 'A non-numeric value encountered'; then
        fail "numeric warning text was not compiled out of $binary"
    fi
    if strings "$binary" | grep -q 'Undefined variable'; then
        fail "undefined-variable warning text was not compiled out of $binary"
    fi
    if nm "$binary" | grep -q 'pphp_warning'; then
        fail "pphp_warning implementation was not compiled out of $binary"
    fi
done
