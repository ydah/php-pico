"""Scalar, operator, cast, and control-flow PHPT families."""

from __future__ import annotations

from .case import Case


def arithmetic_cases() -> list[Case]:
    cases = []
    for index in range(100):
        left = index * 17 % 191 + 11
        right = index * 29 % 23 + 2
        shift = index % 4
        values = [
            left + right,
            left - right,
            left * right,
            left % right,
            (left + right) * 2 - right,
            left << shift,
        ]
        source = (
            f"$a = {left}; $b = {right};\n"
            f"echo $a + $b, ':', $a - $b, ':', $a * $b, ':', "
            f"$a % $b, ':', ($a + $b) * 2 - $b, ':', $a << {shift};"
        )
        cases.append(Case(
            "010_scalar", f"arithmetic_{index:03d}",
            f"integer arithmetic vector {index:03d}", source,
            ":".join(map(str, values)),
        ))
    return cases


def operator_cast_cases() -> list[Case]:
    cases = []
    for index in range(50):
        left = index * 13 % 127 + 1
        right = index * 7 % 63 + 1
        numeric = index * 9 + 3
        expected = ":".join((
            str(left & right), str(left | right), str(left ^ right),
            "1" if left < right else "0", "1" if left >= right else "0",
            str(numeric), str(numeric), "1", "0",
        ))
        source = (
            f"$a = {left}; $b = {right}; $text = '{numeric}.75';\n"
            "echo ($a & $b), ':', ($a | $b), ':', ($a ^ $b), ':', "
            "($a < $b ? 1 : 0), ':', ($a >= $b ? 1 : 0), ':', "
            "(int)$text, ':', (string)" + str(numeric) + ", ':', "
            "((bool)$text ? 1 : 0), ':', ((bool)'0' ? 1 : 0);"
        )
        cases.append(Case(
            "011_operators", f"cast_bitwise_{index:03d}",
            f"casts comparisons and bitwise operators vector {index:03d}",
            source, expected,
        ))
    return cases


def control_cases() -> list[Case]:
    cases = []
    for index in range(50):
        limit = index % 8 + 3
        seed = index * 5 + 1
        total = sum(seed + item for item in range(limit) if item % 3 != 1)
        branch = (seed + limit) % 4
        label = ("zero", "one", "two", "three")[branch]
        source = (
            f"$total = 0; $seed = {seed};\n"
            f"for ($i = 0; $i < {limit}; $i++) {{\n"
            "    if ($i % 3 === 1) { continue; }\n"
            "    $total += $seed + $i;\n"
            "}\n"
            f"switch (($seed + {limit}) % 4) {{\n"
            "case 0: $label = 'zero'; break;\n"
            "case 1: $label = 'one'; break;\n"
            "case 2: $label = 'two'; break;\n"
            "default: $label = 'three';\n"
            "}\n"
            "echo $total, ':', $label;"
        )
        cases.append(Case(
            "012_control", f"loop_switch_{index:03d}",
            f"loop continue and switch vector {index:03d}", source,
            f"{total}:{label}",
        ))
    return cases


def cases() -> list[Case]:
    return arithmetic_cases() + operator_cast_cases() + control_cases()
