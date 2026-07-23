"""Function, default, variadic, closure, and recursion PHPT families."""

from __future__ import annotations

from .case import Case


def default_cases() -> list[Case]:
    cases = []
    for index in range(25):
        default = index * 3 + 2
        supplied = index * 5 + 7
        source = (
            f"function generated_default_{index:03d}($left, $right = {default}) {{\n"
            "    return $left + $right;\n"
            "}\n"
            f"echo generated_default_{index:03d}({supplied}), ':', "
            f"generated_default_{index:03d}({supplied}, {default + 1});"
        )
        cases.append(Case(
            "040_functions", f"default_{index:03d}",
            f"function default argument vector {index:03d}", source,
            f"{supplied + default}:{supplied + default + 1}",
        ))
    return cases


def variadic_cases() -> list[Case]:
    cases = []
    for index in range(20):
        values = [index + offset for offset in range(1, index % 5 + 4)]
        base = index * 2 + 1
        expected = base + sum(values)
        source = (
            f"function generated_variadic_{index:03d}($base, ...$values) {{\n"
            "    return array_reduce($values, fn($sum, $value) => "
            "$sum + $value, $base);\n"
            "}\n"
            f"echo generated_variadic_{index:03d}({base}, "
            f"{', '.join(map(str, values))});"
        )
        cases.append(Case(
            "041_variadic", f"variadic_{index:03d}",
            f"variadic argument collection vector {index:03d}", source,
            str(expected),
        ))
    return cases


def closure_value_cases() -> list[Case]:
    cases = []
    for index in range(25):
        captured = index * 7 + 3
        argument = index * 2 + 5
        source = (
            f"$captured = {captured};\n"
            "$callback = function ($value) use ($captured) { "
            "return $captured + $value; };\n"
            "$captured = 9999;\n"
            f"echo $callback({argument});"
        )
        cases.append(Case(
            "042_closures", f"capture_value_{index:03d}",
            f"closure capture-by-value vector {index:03d}", source,
            str(captured + argument),
        ))
    return cases


def arrow_cases() -> list[Case]:
    cases = []
    for index in range(20):
        initial = index * 4 + 1
        first = index % 5 + 2
        second = index % 7 + 3
        source = (
            f"$base = {initial};\n"
            "$add = fn($value) => $base + $value;\n"
            "$base = 9999;\n"
            f"echo $add({first}), ':', $add({second});"
        )
        cases.append(Case(
            "043_arrow_functions", f"arrow_capture_{index:03d}",
            f"arrow function implicit capture vector {index:03d}", source,
            f"{initial + first}:{initial + second}",
        ))
    return cases


def recursion_cases() -> list[Case]:
    cases = []
    for index in range(20):
        depth = index % 7 + 1
        seed = index * 3 + 2
        source = (
            f"function generated_recursive_{index:03d}($value, $depth) {{\n"
            "    if ($depth === 0) { return $value; }\n"
            f"    return generated_recursive_{index:03d}($value + $depth, "
            "$depth - 1);\n"
            "}\n"
            f"echo generated_recursive_{index:03d}({seed}, {depth});"
        )
        cases.append(Case(
            "044_recursion", f"recursion_{index:03d}",
            f"recursive function vector {index:03d}", source,
            str(seed + depth * (depth + 1) // 2),
        ))
    return cases


def cases() -> list[Case]:
    return (default_cases() + variadic_cases() + closure_value_cases()
            + arrow_cases() + recursion_cases())
