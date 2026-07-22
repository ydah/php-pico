#!/bin/sh
set -eu

typecheck_on=${1:?typecheck-on compiler required}
typecheck_off=${2:?typecheck-off compiler required}
pbc_on=${3:?typecheck-on PBC runtime required}
pbc_off=${4:?typecheck-off PBC runtime required}
no_float=${5:?integer-only typecheck compiler required}
int64=${6:?int64 typecheck compiler required}
rp_equivalent=${7:?RP2040-equivalent typecheck compiler required}
ubsan=${8:?UBSan typecheck compiler required}
asan=${9:?ASan typecheck compiler required}

temporary=$(mktemp -d "${TMPDIR:-/tmp}/php-pico-typecheck.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

fail() {
    echo "typecheck test: $*" >&2
    exit 1
}

assert_output() {
    binary=$1
    source=$2
    expected=$3
    actual=$($binary -r "$source")
    test "$actual" = "$expected" || {
        printf 'expected: %s\nactual: %s\n' "$expected" "$actual" >&2
        fail "unexpected output from $binary"
    }
}

assert_failure() {
    binary=$1
    source=$2
    pattern=$3
    label=$4
    if "$binary" -r "$source" >"$temporary/$label.out" \
            2>"$temporary/$label.err"; then
        fail "$label unexpectedly succeeded"
    fi
    grep -q "$pattern" "$temporary/$label.err" || {
        cat "$temporary/$label.err" >&2
        fail "$label did not report $pattern"
    }
}

scalar_source='declare(strict_types=1);
function scalar_types(int $i, float $f, string $s, bool $b, array $a, null $n, mixed $m): string {
    return $i . ":" . $f . ":" . $s . ":" . ($b ? 1 : 0) . ":" . count($a) . ":" . ($n === null ? 1 : 0) . ":" . $m;
}
echo scalar_types(3, 1.5, "x", true, [1, 2], null, 7);'
assert_output "$typecheck_on" "$scalar_source" '3:1.5:x:1:2:1:7'
assert_output "$rp_equivalent" "$scalar_source" '3:1.5:x:1:2:1:7'
assert_output "$ubsan" "$scalar_source" '3:1.5:x:1:2:1:7'
ASAN_OPTIONS=detect_leaks=0 assert_output "$asan" "$scalar_source" \
    '3:1.5:x:1:2:1:7'

union_source='function union_value(int|string|null $value): string { return gettype($value); }
echo union_value(1), ":", union_value("x"), ":", union_value(null), ":";
function nullable(?int $value): ?int { return $value; }
echo nullable(null) === null ? 1 : 0;'
assert_output "$typecheck_on" "$union_source" 'integer:string:NULL:1'

class_source='interface Marker {}
class Base implements Marker {
    public function same(self $value): self { return $value; }
    public static function create(): static { return new static(); }
}
class Child extends Base {
    public function fromParent(parent $value): parent { return $value; }
}
$child = new Child();
echo get_class($child->same($child)), ":", get_class($child->fromParent(new Base())), ":", get_class(Child::create()), ":";
function marker(Marker $value): Marker { return $value; }
echo get_class(marker($child));'
assert_output "$typecheck_on" "$class_source" 'Child:Base:Child:Child'

callable_source='class Calls { public function method() { return 1; } }
class Invokable { public function __invoke() { return 1; } }
function accepts(callable $value): bool { return true; }
function user_callable() { return 1; }
$calls = new Calls();
echo accepts(function () {}), accepts(fn () => 1), accepts("strlen"), accepts("user_callable"), accepts([$calls, "method"]), accepts(new Invokable());
try { accepts("missing_callable"); } catch (TypeError $error) { echo ":", get_class($error); }'
assert_output "$typecheck_on" "$callable_source" '111111:TypeError'

binding_source='function defaults(int $value = 4): int { return $value; }
function variadic(int ...$values): int { return count($values); }
$closure = function (string $value): string { return $value; };
$arrow = fn (int $value): int => $value;
class Binding {
    public static function staticValue(bool $value): bool { return $value; }
    public function method(array $value): array { return $value; }
    public function __construct(public readonly int $id) {}
}
$binding = new Binding(9);
echo defaults(), ":", variadic(1, 2, 3), ":", $closure("x"), ":", $arrow(5), ":", Binding::staticValue(true), ":", count($binding->method([1, 2])), ":", $binding->id;'
assert_output "$typecheck_on" "$binding_source" '4:3:x:5:1:2:9'

error_source='function typed_argument(int $value): int { return $value; }
try { typed_argument("x"); } catch (TypeError $error) { echo get_class($error), ":arg:"; }
function typed_return(): int { return "x"; }
try { typed_return(); } catch (TypeError $error) { echo get_class($error), ":ret:"; }
function final_return(): int { try { return 1; } finally { return "x"; } }
try { final_return(); } catch (TypeError $error) { echo get_class($error), ":finally:"; }
function variadic_error(int ...$values): int { return 1; }
try { variadic_error(1, "x"); } catch (TypeError $error) { echo get_class($error), ":variadic"; }'
assert_output "$typecheck_on" "$error_source" 'TypeError:arg:TypeError:ret:TypeError:finally:TypeError:variadic'

property_source='class Properties {
    public int $instance = 1;
    public static ?string $staticValue = null;
    public array $uninitialized;
}
$object = new Properties();
echo $object->instance, ":";
try { $object->instance = "x"; } catch (TypeError $error) { echo "instance:"; }
Properties::$staticValue = "ok";
echo Properties::$staticValue, ":";
try { Properties::$staticValue = 3; } catch (TypeError $error) { echo "static:"; }
try { echo $object->uninitialized; } catch (Error $error) { echo "uninitialized"; }'
assert_output "$typecheck_on" "$property_source" '1:instance:ok:static:uninitialized'
assert_output "$typecheck_on" \
    'try { class BadDefault { public int $value = null; } } catch (TypeError $error) { echo "caught:"; } class GoodAfter { public int $value = 1; } echo (new GoodAfter())->value;' \
    'caught:1'

off_source='function unchecked(int $value): int { return "result"; }
class Unchecked { public int $value = null; }
$object = new Unchecked();
$object->value = "free";
echo unchecked("argument"), ":", $object->value;'
assert_output "$typecheck_off" "$off_source" 'result:free'

assert_failure "$typecheck_on" \
    'function bad(int $value): int { return $value; } bad("x");' \
    'Uncaught TypeError' uncaught
assert_failure "$typecheck_on" 'function bad(void $value) {}' \
    'void is only valid as a return type' void-parameter
assert_failure "$typecheck_on" 'function bad(): int|void {}' \
    'void cannot be part of a union' void-union
assert_failure "$typecheck_on" 'function bad(): mixed|int {}' \
    'mixed cannot be part of a union' mixed-union
assert_failure "$typecheck_on" 'function bad(int|int $value) {}' \
    'duplicate type in union' duplicate
assert_failure "$typecheck_on" 'function bad(?mixed $value) {}' \
    'cannot use nullable shorthand' nullable-mixed
assert_failure "$typecheck_on" 'function bad(?int|string $value) {}' \
    'nullable shorthand cannot be combined with a union' nullable-union
assert_failure "$typecheck_on" 'class Bad { public callable $value; }' \
    'callable is not valid as a property type' callable-property
assert_failure "$no_float" 'function bad(float $value) {}' \
    'float type declarations require PPHP_ENABLE_FLOAT=1' no-float-type

assert_output "$int64" \
    'function wide(int $value): int { return $value; } echo wide(2147483648);' \
    '2147483648'

cat >"$temporary/typed.php" <<'PHP'
<?php
interface PbcMarker {}
class PbcValue implements PbcMarker {
    public string $name = "pbc";
    public function identity(self $value): self { return $value; }
}
function pbc_typed(PbcMarker $value): string { return $value->name; }
$value = new PbcValue();
echo pbc_typed($value), ":", $value->identity($value)->name, ":";
try { pbc_typed("bad"); } catch (TypeError $error) { echo get_class($error); }
PHP
"$typecheck_on" -c "$temporary/typed.php" -o "$temporary/typed.pbc"
test "$("$pbc_on" "$temporary/typed.pbc")" = 'pbc:pbc:TypeError' ||
    fail 'type metadata was not enforced by the compiler-free runtime'
ASAN_OPTIONS=detect_leaks=0 "$asan" "$temporary/typed.pbc" \
    >"$temporary/asan-pbc.out"
test "$(cat "$temporary/asan-pbc.out")" = 'pbc:pbc:TypeError' ||
    fail 'ASan runtime changed typed PBC execution'

if "$pbc_off" "$temporary/typed.pbc" >"$temporary/mismatch.out" \
        2>"$temporary/mismatch.err"; then
    fail 'typecheck-off runtime accepted typecheck-on PBC'
fi
grep -q 'invalid or incompatible PBC image' "$temporary/mismatch.err" ||
    fail 'PBC configuration mismatch was not reported'

cp "$temporary/typed.pbc" "$temporary/corrupt.pbc"
printf '\006' | dd of="$temporary/corrupt.pbc" bs=1 seek=6 conv=notrunc \
    2>/dev/null
if "$pbc_on" "$temporary/corrupt.pbc" >"$temporary/corrupt.out" \
        2>"$temporary/corrupt.err"; then
    fail 'typecheck-on runtime accepted corrupt PBC flags'
fi
grep -q 'invalid or incompatible PBC image' "$temporary/corrupt.err" ||
    fail 'corrupt PBC was not rejected'

echo 'typecheck tests passed'
