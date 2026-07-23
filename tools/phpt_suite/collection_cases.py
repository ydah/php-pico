"""Array, key-order, callback, and nested COW PHPT families."""

from __future__ import annotations

from .case import Case


def cow_cases() -> list[Case]:
    cases = []
    for index in range(50):
        values = [index + 1, index * 2 + 3, index * 3 + 5]
        replacement = index * 7 + 11
        source = (
            f"$original = [{', '.join(map(str, values))}];\n"
            "$copy = $original;\n"
            f"$copy[1] = {replacement}; $copy[] = {replacement + 1};\n"
            "echo implode(',', $original), ':', implode(',', $copy);"
        )
        expected = (
            f"{','.join(map(str, values))}:"
            f"{values[0]},{replacement},{values[2]},{replacement + 1}"
        )
        cases.append(Case(
            "020_arrays", f"cow_{index:03d}",
            f"array copy-on-write vector {index:03d}", source, expected,
        ))
    return cases


def key_order_cases() -> list[Case]:
    cases = []
    for index in range(40):
        first = index + 10
        second = index * 3 + 20
        third = index * 5 + 30
        source = (
            f"$items = ['left' => {first}, 4 => {second}, 'right' => {third}];\n"
            "unset($items[4]);\n"
            f"$items['tail'] = {third + 1};\n"
            "$keys = array_keys($items); $values = array_values($items);\n"
            "echo implode(',', $keys), ':', implode(',', $values), ':', "
            "(array_key_exists('right', $items) ? 1 : 0);"
        )
        cases.append(Case(
            "021_array_keys", f"key_order_{index:03d}",
            f"associative key order vector {index:03d}", source,
            f"left,right,tail:{first},{third},{third + 1}:1",
        ))
    return cases


def callback_cases() -> list[Case]:
    cases = []
    for index in range(40):
        values = [index + offset for offset in range(1, 6)]
        factor = index % 4 + 2
        mapped = [value * factor for value in values]
        filtered = [value for value in mapped if value % 3 != 0]
        total = sum(filtered)
        source = (
            f"$values = [{', '.join(map(str, values))}]; $factor = {factor};\n"
            "$mapped = array_map(fn($value) => $value * $factor, $values);\n"
            "$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);\n"
            "$total = array_reduce($filtered, fn($carry, $value) => "
            "$carry + $value, 0);\n"
            "echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;"
        )
        cases.append(Case(
            "022_array_callbacks", f"map_filter_reduce_{index:03d}",
            f"array callbacks vector {index:03d}", source,
            f"{','.join(map(str, mapped))}:{','.join(map(str, filtered))}:{total}",
        ))
    return cases


def nested_cases() -> list[Case]:
    cases = []
    for index in range(40):
        initial = index * 4 + 1
        increment = index % 7 + 2
        source = (
            f"$tree = ['node' => ['value' => {initial}, 'items' => [{initial + 1}]]];\n"
            "$copy = $tree;\n"
            f"$copy['node']['value'] += {increment};\n"
            f"$copy['node']['items'][] = {initial + increment + 2};\n"
            "echo $tree['node']['value'], ':', $copy['node']['value'], ':', "
            "implode(',', $tree['node']['items']), ':', "
            "implode(',', $copy['node']['items']);"
        )
        cases.append(Case(
            "023_array_nested", f"nested_cow_{index:03d}",
            f"nested array copy-on-write vector {index:03d}", source,
            f"{initial}:{initial + increment}:{initial + 1}:"
            f"{initial + 1},{initial + increment + 2}",
        ))
    return cases


def cases() -> list[Case]:
    return cow_cases() + key_order_cases() + callback_cases() + nested_cases()
