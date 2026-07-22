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
no_float_off=${10:?integer-only non-typecheck compiler required}

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
assert_no_float_declarations() {
    binary=$1
    suffix=$2
    assert_failure "$binary" 'function bad(float $value) {}' \
        'float type declarations require PPHP_ENABLE_FLOAT=1' "no-float-type-$suffix"
    assert_failure "$binary" 'class Bad { public float $value; }' \
        'float type declarations require PPHP_ENABLE_FLOAT=1' "no-float-property-$suffix"
    assert_failure "$binary" 'class Bad { public function value(): float {} }' \
        'float type declarations require PPHP_ENABLE_FLOAT=1' "no-float-method-$suffix"
    assert_failure "$binary" '$bad = function (float $value) {};' \
        'float type declarations require PPHP_ENABLE_FLOAT=1' "no-float-closure-$suffix"
    assert_failure "$binary" '$bad = fn (): float => 1;' \
        'float type declarations require PPHP_ENABLE_FLOAT=1' "no-float-arrow-$suffix"
    assert_failure "$binary" \
        'class Bad { public function __construct(public float $value) {} }' \
        'float type declarations require PPHP_ENABLE_FLOAT=1' "no-float-promoted-$suffix"
}
assert_no_float_declarations "$no_float" on
assert_no_float_declarations "$no_float_off" off

context_source='class ContextBase {}
class ContextChild extends ContextBase {
    public function same(self $value): self { return $value; }
    public function parentValue(parent $value): parent { return $value; }
    public function closures(): self {
        $closure = function (self $value): self { return $value; };
        $arrow = fn (parent $value): parent => $value;
        $closure($this); $arrow(new ContextBase()); return $this;
    }
    public function __CONSTRUCT(): void {}
}
echo get_class((new ContextChild())->closures());'
assert_output "$typecheck_on" "$context_source" 'ContextChild'
assert_output "$typecheck_off" "$context_source" 'ContextChild'
assert_failure "$typecheck_on" 'function bad(): self {}' \
    'self type requires class scope' top-self
assert_failure "$typecheck_off" 'function bad(): self {}' \
    'self type requires class scope' top-self-off
assert_failure "$typecheck_on" '$bad = function (): parent {};' \
    'parent type requires class scope' closure-parent
assert_failure "$typecheck_on" '$bad = fn (): static => null;' \
    'static type is only valid as a method return type' arrow-static
assert_failure "$typecheck_on" 'class Bad { public function value(): parent {} }' \
    'parent type requires a parent class' missing-parent
assert_failure "$typecheck_off" 'class Bad { public function value(): parent {} }' \
    'parent type requires a parent class' missing-parent-off
assert_failure "$typecheck_off" 'function bad(): void { return 1; }' \
    'a void function must not return a value' void-return-off
assert_failure "$typecheck_on" 'class Bad { public function __Construct() { return 1; } }' \
    'a void function must not return a value' constructor-return
assert_failure "$typecheck_off" 'class Bad { public function __Construct() { return 1; } }' \
    'a void function must not return a value' constructor-return-off
assert_failure "$typecheck_on" 'class Bad { public function __DESTRUCT() { return 1; } }' \
    'a void function must not return a value' destructor-return
assert_failure "$typecheck_on" 'class Bad { public function __construct(): int {} }' \
    'constructor may only declare void' constructor-type
assert_failure "$typecheck_off" 'class Bad { public function __construct(): int {} }' \
    'constructor may only declare void' constructor-type-off

variance_source='interface VariantContract {
    public function convert(VariantBase|int $value): VariantBase;
}
class VariantBase {}
class VariantChild extends VariantBase implements VariantContract {
    public function convert(mixed $value): VariantChild { return new VariantChild(); }
}
class VariantGrandchild extends VariantChild {
    public function convert(mixed $value, int $extra = 0): VariantGrandchild {
        return new VariantGrandchild();
    }
}
echo get_class((new VariantGrandchild())->convert("ok"));'
assert_output "$typecheck_on" "$variance_source" 'VariantGrandchild'
assert_failure "$typecheck_on" \
    'class A { public function f(int|string $v): A { return $this; } } class B extends A { public function f(int $v): A { return $this; } }' \
    'must implement method f' variance-parameter
assert_failure "$typecheck_on" \
    'class A { public function f(int $v): A { return $this; } } class B extends A { public function f(int $v): mixed { return $this; } }' \
    'must implement method f' variance-return
assert_failure "$typecheck_on" \
    'interface I { public function f(int $v): int; } class C implements I { public function f(int $v, int $required): int { return 1; } }' \
    'must implement method f' variance-arity
assert_failure "$typecheck_on" \
    'interface I { public function f(int ...$v): int; } class C implements I { public function f(int $v): int { return 1; } }' \
    'must implement method f' variance-variadic

readonly_source='class ReadonlyPromotion {
    public function __construct(public readonly int $id = 7) {}
}
$value = new ReadonlyPromotion(); $copy = clone $value;
echo $value->id, ":", $copy->id, ":";
try { $value->{"id"} = 8; } catch (Error $error) { echo "dynamic:"; }
class ReadonlyChild extends ReadonlyPromotion {}
try { (new ReadonlyChild())->id = 9; } catch (Error $error) { echo "inherited"; }'
assert_output "$typecheck_on" "$readonly_source" '7:7:dynamic:inherited'
assert_failure "$typecheck_off" \
    'class Bad { public function method(public int $value) {} }' \
    'property promotion is only valid in a constructor' promotion-context
assert_failure "$typecheck_off" \
    'class Bad { public readonly $value; }' \
    'a readonly property must have a type' readonly-untyped
assert_failure "$typecheck_off" \
    'class Bad { public readonly int $value = 1; }' \
    'a readonly property cannot have a default value' readonly-default
assert_failure "$typecheck_off" \
    'class Bad { public static readonly int $value; }' \
    'a readonly property cannot be static' readonly-static
assert_failure "$typecheck_off" \
    'class Bad { public function __construct(public readonly $value) {} }' \
    'a readonly property must have a type' readonly-promoted-untyped
assert_failure "$typecheck_off" \
    'readonly class Base {} class Bad extends Base {}' \
    'readonly classes may only extend readonly classes' readonly-parent
assert_failure "$typecheck_off" \
    'class Base {} readonly class Bad extends Base {}' \
    'readonly classes may only extend readonly classes' readonly-child

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
interface PbcVariant {
    public function convert(PbcValue|int $value): PbcValue;
}
class PbcChild extends PbcValue implements PbcVariant {
    public function convert(mixed $value): self { return $this; }
}
readonly class PbcReadonly {
    public function __construct(public int $id = 4) {}
}
function pbc_void_finally(): void {
    try { return; } finally {}
}
class PbcVoidConstructor {
    public function __construct() {
        try { return; } finally {}
    }
}
function pbc_typed(PbcMarker $value): string { return $value->name; }
$value = new PbcValue();
echo pbc_typed($value), ":", $value->identity($value)->name, ":";
try { pbc_typed("bad"); } catch (TypeError $error) { echo get_class($error); }
pbc_void_finally(); new PbcVoidConstructor();
echo ":", get_class((new PbcChild())->convert("ok")), ":", (new PbcReadonly())->id;
PHP
"$typecheck_on" -c "$temporary/typed.php" -o "$temporary/typed.pbc"
test "$("$pbc_on" "$temporary/typed.pbc")" = 'pbc:pbc:TypeError:PbcChild:4' ||
    fail 'type metadata was not enforced by the compiler-free runtime'
ASAN_OPTIONS=detect_leaks=0 "$asan" "$temporary/typed.pbc" \
    >"$temporary/asan-pbc.out"
test "$(cat "$temporary/asan-pbc.out")" = 'pbc:pbc:TypeError:PbcChild:4' ||
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

cat >"$temporary/variance-bad.php" <<'PHP'
<?php
class PbcVarianceBase {
    public function value(int|string $value): int { return 1; }
}
class PbcVarianceBad extends PbcVarianceBase {
    public function value(int $value): int { return 1; }
}
PHP
"$typecheck_on" -c "$temporary/variance-bad.php" \
    -o "$temporary/variance-bad.pbc"
if "$pbc_on" "$temporary/variance-bad.pbc" \
        >"$temporary/variance-bad.out" 2>"$temporary/variance-bad.err"; then
    fail 'compiler-free runtime accepted incompatible method variance'
fi
grep -q 'must implement method value' "$temporary/variance-bad.err" ||
    fail 'compiler-free runtime did not validate method variance'

printf '<?php echo 1;\n' >"$temporary/float-config.php"
"$typecheck_off" -c "$temporary/float-config.php" \
    -o "$temporary/float-config.pbc"
if "$no_float_off" "$temporary/float-config.pbc" \
        >"$temporary/float-config.out" 2>"$temporary/float-config.err"; then
    fail 'integer-only runtime accepted float-enabled PBC'
fi
grep -q 'invalid or incompatible PBC image' "$temporary/float-config.err" ||
    fail 'float PBC configuration mismatch was not reported'

echo 'typecheck tests passed'
