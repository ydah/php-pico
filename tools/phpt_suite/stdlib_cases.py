"""Deterministic JSON, conversion, and array-helper PHPT families."""

from __future__ import annotations

from .case import Case


def json_cases() -> list[Case]:
    cases = []
    for index in range(20):
        name = f"item{index:02d}"
        number = index * 13 + 1
        truth = index % 2 == 0
        truth_php = "true" if truth else "false"
        truth_json = "true" if truth else "false"
        encoded = f'{{"name":"{name}","number":{number},"enabled":{truth_json}}}'
        source = (
            f"$value = ['name' => '{name}', 'number' => {number}, "
            f"'enabled' => {truth_php}];\n"
            "$json = json_encode($value); $decoded = json_decode($json, true);\n"
            "echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', "
            "($decoded['enabled'] ? 1 : 0);"
        )
        cases.append(Case(
            "060_json", f"roundtrip_{index:03d}",
            f"JSON encode and decode vector {index:03d}", source,
            f"{encoded}:{name}:{number}:{1 if truth else 0}",
        ))
    return cases


def conversion_cases() -> list[Case]:
    cases = []
    for index in range(14):
        value = index * 17 + 16
        hexadecimal = format(value, "x")
        binary = format(value, "b")
        octal = format(value, "o")
        source = (
            f"echo dechex({value}), ':', hexdec('{hexadecimal}'), ':', "
            f"decbin({value}), ':', bindec('{binary}'), ':', "
            f"decoct({value}), ':', octdec('{octal}');"
        )
        cases.append(Case(
            "061_conversions", f"base_{index:03d}",
            f"integer base conversion vector {index:03d}", source,
            f"{hexadecimal}:{value}:{binary}:{value}:{octal}:{value}",
        ))
    return cases


def array_helper_cases() -> list[Case]:
    cases = []
    for index in range(20):
        values = [index + 3, index * 2 + 7, index * 3 + 11]
        reverse = list(reversed(values))
        total = sum(values)
        product = values[0] * values[1] * values[2]
        source = (
            f"$values = [{', '.join(map(str, values))}];\n"
            "$reverse = array_reverse($values); $slice = array_slice($values, 1, 2);\n"
            "echo implode(',', $reverse), ':', implode(',', $slice), ':', "
            "array_sum($values), ':', array_product($values), ':', "
            "max($values), ':', min($values);"
        )
        cases.append(Case(
            "062_array_helpers", f"helpers_{index:03d}",
            f"array helper vector {index:03d}", source,
            f"{','.join(map(str, reverse))}:{values[1]},{values[2]}:"
            f"{total}:{product}:{max(values)}:{min(values)}",
        ))
    return cases


def cases() -> list[Case]:
    return json_cases() + conversion_cases() + array_helper_cases()
