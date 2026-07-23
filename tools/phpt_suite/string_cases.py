"""String interpolation, replacement, search, and formatting PHPT families."""

from __future__ import annotations

from .case import Case


def interpolation_cases() -> list[Case]:
    cases = []
    for index in range(40):
        word = f"pico{index:02d}"
        number = index * 11 + 7
        source = (
            f"$word = '{word}'; $number = {number};\n"
            "echo \"[$word:$number]\", ':', $word . '-' . $number, ':', "
            "strlen($word);"
        )
        cases.append(Case(
            "030_strings", f"interpolation_{index:03d}",
            f"string interpolation and concatenation vector {index:03d}",
            source, f"[{word}:{number}]:{word}-{number}:{len(word)}",
        ))
    return cases


def replacement_cases() -> list[Case]:
    cases = []
    for index in range(40):
        token = f"t{index:02d}"
        replacement = f"R{index:02d}"
        repeat = index % 4 + 2
        subject = (token + "x") * repeat + token
        replaced = subject.replace(token, replacement)
        source = (
            f"$subject = '{subject}';\n"
            f"echo str_replace('{token}', '{replacement}', $subject), ':', "
            f"str_repeat('{token}', {repeat}), ':', strrev($subject);"
        )
        cases.append(Case(
            "031_string_replace", f"replace_{index:03d}",
            f"string replacement repetition and reversal vector {index:03d}",
            source,
            f"{replaced}:{token * repeat}:{subject[::-1]}",
        ))
    return cases


def search_cases() -> list[Case]:
    cases = []
    for index in range(30):
        prefix = chr(ord("a") + index % 10) * (index % 3 + 1)
        middle = f"needle{index:02d}"
        suffix = chr(ord("k") + index % 10) * (index % 4 + 1)
        subject = prefix + middle + suffix
        position = len(prefix)
        source = (
            f"$text = '{subject}';\n"
            f"echo strpos($text, '{middle}'), ':', "
            f"(str_contains($text, '{middle}') ? 1 : 0), ':', "
            f"(str_starts_with($text, '{prefix}') ? 1 : 0), ':', "
            f"(str_ends_with($text, '{suffix}') ? 1 : 0), ':', "
            f"substr($text, {position}, {len(middle)});"
        )
        cases.append(Case(
            "032_string_search", f"search_{index:03d}",
            f"string search and slice vector {index:03d}", source,
            f"{position}:1:1:1:{middle}",
        ))
    return cases


def split_format_cases() -> list[Case]:
    cases = []
    for index in range(30):
        values = [f"v{index + offset}" for offset in range(3)]
        joined = ",".join(values)
        width = len(values[0]) + index % 4 + 2
        padded = values[0].ljust(width, "-")
        source = (
            f"$parts = explode(',', '{joined}');\n"
            f"echo implode('|', $parts), ':', count($parts), ':', "
            f"strtoupper($parts[1]), ':', strtolower('MIX{index}'), ':', "
            f"str_pad($parts[0], {width}, '-');"
        )
        cases.append(Case(
            "033_string_split", f"split_format_{index:03d}",
            f"string split transform and padding vector {index:03d}", source,
            f"{'|'.join(values)}:3:{values[1].upper()}:mix{index}:{padded}",
        ))
    return cases


def cases() -> list[Case]:
    return interpolation_cases() + replacement_cases() + search_cases() + split_format_cases()
