"""Exception and explicit finally-control-flow PHPT families."""

from __future__ import annotations

from .case import Case


def catch_cases() -> list[Case]:
    cases = []
    for index in range(10):
        message = f"caught-{index:02d}"
        code = index * 7 + 3
        source = (
            f"try {{ throw new Exception('{message}', {code}); }}\n"
            "catch (Exception $error) {\n"
            "    echo $error->getMessage(), ':', $error->getCode();\n"
            "}"
        )
        cases.append(Case(
            "070_exceptions", f"catch_{index:03d}",
            f"exception catch message and code vector {index:03d}", source,
            f"{message}:{code}",
        ))
    return cases


def propagation_cases() -> list[Case]:
    cases = []
    for index in range(10):
        value = index * 9 + 4
        source = (
            f"function generated_throw_{index:03d}($value) {{\n"
            "    if ($value > 0) { throw new RuntimeException('value=' . $value); }\n"
            "    return 0;\n"
            "}\n"
            f"try {{ generated_throw_{index:03d}({value}); }}\n"
            "catch (RuntimeException $error) { echo 'runtime:', $error->getMessage(); }"
        )
        cases.append(Case(
            "071_exception_flow", f"propagation_{index:03d}",
            f"exception propagation through a function vector {index:03d}",
            source, f"runtime:value={value}",
        ))
    return cases


def finally_cases() -> list[Case]:
    definitions = [
        (
            "01_normal_completion", "finally after normal completion",
            "$log = [];\ntry { $log[] = 'try'; }\n"
            "finally { $log[] = 'finally'; }\necho implode(',', $log);",
            "try,finally",
        ),
        (
            "02_return_path", "finally executes on a return path",
            "function final_return_path() {\n"
            "    try { return 7; } finally { echo 'finally:'; }\n"
            "}\necho final_return_path();",
            "finally:7",
        ),
        (
            "03_finally_return_override", "finally return overrides try return",
            "function final_return_override() {\n"
            "    try { return 3; } finally { return 9; }\n"
            "}\necho final_return_override();",
            "9",
        ),
        (
            "04_throw_propagation", "finally runs before a throw propagates",
            "try {\n"
            "    try { throw new Exception('boom'); } finally { echo 'finally:'; }\n"
            "} catch (Exception $error) { echo $error->getMessage(); }",
            "finally:boom",
        ),
        (
            "05_return_overrides_throw", "finally return overrides a pending throw",
            "function final_return_over_throw() {\n"
            "    try { throw new Exception('discarded'); } finally { return 12; }\n"
            "}\necho final_return_over_throw();",
            "12",
        ),
        (
            "06_throw_overrides_return", "finally throw overrides a pending return",
            "function final_throw_over_return() {\n"
            "    try { return 4; } finally { throw new Exception('override'); }\n"
            "}\ntry { final_throw_over_return(); }\n"
            "catch (Exception $error) { echo $error->getMessage(); }",
            "override",
        ),
        (
            "07_break_path", "finally executes before loop break",
            "$log = [];\nfor ($i = 0; $i < 4; $i++) {\n"
            "    try { $log[] = 'try' . $i; if ($i === 1) { break; } }\n"
            "    finally { $log[] = 'finally' . $i; }\n"
            "}\necho implode(',', $log);",
            "try0,finally0,try1,finally1",
        ),
        (
            "08_continue_path", "finally executes before loop continue",
            "$log = [];\nfor ($i = 0; $i < 3; $i++) {\n"
            "    try { $log[] = 'try' . $i; if ($i < 2) { continue; } }\n"
            "    finally { $log[] = 'finally' . $i; }\n"
            "    $log[] = 'body' . $i;\n"
            "}\necho implode(',', $log);",
            "try0,finally0,try1,finally1,try2,finally2,body2",
        ),
        (
            "09_nested_order", "nested finally blocks run inside out",
            "$log = [];\ntry {\n"
            "    try { $log[] = 'body'; } finally { $log[] = 'inner'; }\n"
            "} finally { $log[] = 'outer'; }\necho implode(',', $log);",
            "body,inner,outer",
        ),
        (
            "10_catch_and_finally", "catch executes before sibling finally",
            "$log = [];\ntry { throw new Exception('x'); }\n"
            "catch (Exception $error) { $log[] = 'catch'; }\n"
            "finally { $log[] = 'finally'; }\necho implode(',', $log);",
            "catch,finally",
        ),
        (
            "11_catch_return", "finally executes after return from catch",
            "function final_catch_return() {\n"
            "    try { throw new Exception('x'); }\n"
            "    catch (Exception $error) { return 'catch'; }\n"
            "    finally { echo 'finally:'; }\n"
            "}\necho final_catch_return();",
            "finally:catch",
        ),
        (
            "12_catch_rethrow", "finally executes while catch rethrows",
            "try {\n"
            "    try { throw new Exception('first'); }\n"
            "    catch (Exception $error) { throw new RuntimeException('second'); }\n"
            "    finally { echo 'finally:'; }\n"
            "} catch (RuntimeException $error) { echo $error->getMessage(); }",
            "finally:second",
        ),
        (
            "13_inner_caught_throw", "outer finally runs after inner throw is caught",
            "$log = [];\ntry {\n"
            "    try { throw new Exception('x'); }\n"
            "    catch (Exception $error) { $log[] = 'inner-catch'; }\n"
            "    $log[] = 'after-catch';\n"
            "} finally { $log[] = 'outer-finally'; }\n"
            "echo implode(',', $log);",
            "inner-catch,after-catch,outer-finally",
        ),
        (
            "14_return_expression_order", "return expression evaluates before finally",
            "function final_mark($label) { echo $label; return $label; }\n"
            "function final_expression_order() {\n"
            "    try { return final_mark('A'); } finally { final_mark('B'); }\n"
            "}\n$result = final_expression_order(); echo ':', $result;",
            "AB:A",
        ),
        (
            "15_global_mutation", "finally mutation of global state is visible",
            "$final_global = 2;\nfunction final_mutate_global() {\n"
            "    global $final_global;\n"
            "    try { $final_global += 3; } finally { $final_global *= 4; }\n"
            "}\nfinal_mutate_global(); echo $final_global;",
            "20",
        ),
        (
            "16_closure_method", "finally runs in a closure returned by a method",
            "class FinalClosureFactory {\n"
            "    public function make($base) {\n"
            "        return function ($step) use ($base) {\n"
            "            try { return $base + $step; } finally { echo 'finally:'; }\n"
            "        };\n"
            "    }\n"
            "}\n$callback = (new FinalClosureFactory())->make(30); echo $callback(12);",
            "finally:42",
        ),
        (
            "17_nested_loop_break", "finally executes on an inner nested-loop break",
            "$log = [];\nfor ($outer = 0; $outer < 2; $outer++) {\n"
            "    for ($inner = 0; $inner < 3; $inner++) {\n"
            "        try { $log[] = $outer . $inner; if ($inner === 1) { break; } }\n"
            "        finally { $log[] = 'f' . $outer . $inner; }\n"
            "    }\n"
            "}\necho implode(',', $log);",
            "00,f00,01,f01,10,f10,11,f11",
        ),
        (
            "18_nested_loop_continue", "finally executes on nested-loop continue",
            "$log = [];\nfor ($outer = 0; $outer < 2; $outer++) {\n"
            "    for ($inner = 0; $inner < 2; $inner++) {\n"
            "        try { if ($inner === 0) { $log[] = 'skip' . $outer; continue; } "
            "$log[] = 'keep' . $outer; }\n"
            "        finally { $log[] = 'f' . $outer . $inner; }\n"
            "    }\n"
            "}\necho implode(',', $log);",
            "skip0,f00,keep0,f01,skip1,f10,keep1,f11",
        ),
    ]
    return [
        Case("072_finally", slug, title, source, expected)
        for slug, title, source, expected in definitions
    ]


def cases() -> list[Case]:
    return catch_cases() + propagation_cases() + finally_cases()
