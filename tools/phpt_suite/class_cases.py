"""Class, inheritance, static, visibility, clone, and magic PHPT families."""

from __future__ import annotations

from .case import Case


def inheritance_cases() -> list[Case]:
    cases = []
    for index in range(20):
        base = index * 5 + 3
        extra = index % 6 + 1
        source = (
            f"class GeneratedBase{index:03d} {{\n"
            f"    protected $value = {base};\n"
            "    public function value() { return $this->value; }\n"
            "}\n"
            f"class GeneratedChild{index:03d} extends GeneratedBase{index:03d} {{\n"
            f"    public function value() {{ return parent::value() + {extra}; }}\n"
            "}\n"
            f"$object = new GeneratedChild{index:03d}(); echo $object->value();"
        )
        cases.append(Case(
            "050_classes", f"inheritance_{index:03d}",
            f"class inheritance and parent call vector {index:03d}", source,
            str(base + extra),
        ))
    return cases


def static_cases() -> list[Case]:
    cases = []
    for index in range(15):
        initial = index * 3 + 1
        first = index % 4 + 2
        second = index % 5 + 3
        source = (
            f"class GeneratedStatic{index:03d} {{\n"
            f"    public const OFFSET = {index + 1};\n"
            f"    public static $value = {initial};\n"
            "    public static function add($step) { "
            "self::$value += $step; return self::$value + self::OFFSET; }\n"
            "}\n"
            f"echo GeneratedStatic{index:03d}::add({first}), ':', "
            f"GeneratedStatic{index:03d}::add({second}), ':', "
            f"GeneratedStatic{index:03d}::$value;"
        )
        one_value = initial + first
        two_value = one_value + second
        cases.append(Case(
            "051_class_static", f"static_{index:03d}",
            f"static property method and constant vector {index:03d}", source,
            f"{one_value + index + 1}:{two_value + index + 1}:{two_value}",
        ))
    return cases


def visibility_cases() -> list[Case]:
    cases = []
    for index in range(15):
        private = index * 7 + 4
        protected = index * 5 + 6
        source = (
            f"class GeneratedVisibility{index:03d} {{\n"
            f"    private $left = {private}; protected $right = {protected};\n"
            "    private function left() { return $this->left; }\n"
            "    protected function right() { return $this->right; }\n"
            "    public function total() { return $this->left() + $this->right(); }\n"
            "}\n"
            f"echo (new GeneratedVisibility{index:03d}())->total();"
        )
        cases.append(Case(
            "052_visibility", f"visibility_{index:03d}",
            f"private and protected member access vector {index:03d}", source,
            str(private + protected),
        ))
    return cases


def clone_cases() -> list[Case]:
    cases = []
    for index in range(15):
        first = index + 3
        second = index * 2 + 5
        replacement = index * 9 + 8
        source = (
            f"class GeneratedClone{index:03d} {{ public $items; "
            "public function __construct($items) { $this->items = $items; } }\n"
            f"$original = new GeneratedClone{index:03d}([{first}, {second}]);\n"
            "$copy = clone $original;\n"
            f"$copy->items[0] = {replacement};\n"
            "echo implode(',', $original->items), ':', implode(',', $copy->items);"
        )
        cases.append(Case(
            "053_clone", f"clone_{index:03d}",
            f"object clone and property COW vector {index:03d}", source,
            f"{first},{second}:{replacement},{second}",
        ))
    return cases


def magic_cases() -> list[Case]:
    cases = []
    for index in range(15):
        value = index * 11 + 2
        source = (
            f"class GeneratedMagic{index:03d} {{\n"
            f"    private $value = {value};\n"
            "    public function __get($name) { return $name . ':' . $this->value; }\n"
            "    public function __set($name, $value) { $this->value = $value; }\n"
            "    public function __toString() { return 'box=' . $this->value; }\n"
            "}\n"
            f"$box = new GeneratedMagic{index:03d}(); echo $box->missing, ':';\n"
            f"$box->dynamic = {value + 3}; echo $box;"
        )
        cases.append(Case(
            "054_magic", f"magic_{index:03d}",
            f"magic get set and string conversion vector {index:03d}", source,
            f"missing:{value}:box={value + 3}",
        ))
    return cases


def cases() -> list[Case]:
    return (inheritance_cases() + static_cases() + visibility_cases()
            + clone_cases() + magic_cases())
