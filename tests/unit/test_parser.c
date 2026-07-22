#include "test.h"

#include "alloc.h"
#include "parser.h"

#include <stdint.h>

static uint8_t parser_pool[256U * 1024U];

static pc_ast *parse_source(const char *source, pc_arena *arena, pc_parser *parser) {
    pphp_pool_init(parser_pool, sizeof(parser_pool));
    pc_arena_init(arena, 2048U);
    pc_parser_init(parser, arena, source, strlen(source), 1);
    return pc_parse_program(parser);
}

TEST(operator_precedence_matches_php_8) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source("1 + 2 * 3 . 4 ?? 5;", &arena, &parser);
    pc_ast *expression;
    ASSERT_TRUE(program != NULL);
    expression = program->as.list.items->as.expression.expression;
    ASSERT_EQ(AST_BINARY, expression->kind);
    ASSERT_EQ(T_COALESCE, expression->as.binary.op);
    ASSERT_EQ(T_DOT, expression->as.binary.left->as.binary.op);
    ASSERT_EQ(T_PLUS, expression->as.binary.left->as.binary.left->as.binary.op);
    ASSERT_EQ(T_STAR,
              expression->as.binary.left->as.binary.left->as.binary.right->as.binary.op);
    pc_arena_destroy(&arena);
}

TEST(assignments_are_right_associative_and_validate_lvalues) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source("$a = $b = 3;", &arena, &parser);
    pc_ast *assignment;
    ASSERT_TRUE(program != NULL);
    assignment = program->as.list.items->as.expression.expression;
    ASSERT_EQ(AST_ASSIGN, assignment->kind);
    ASSERT_EQ(AST_ASSIGN, assignment->as.binary.right->kind);
    pc_arena_destroy(&arena);

    program = parse_source("(1 + 2) = 3;", &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "not assignable") != NULL);
    pc_arena_destroy(&arena);
}

TEST(control_flow_and_functions_build_structured_ast) {
    const char *source =
        "function add(int $a, $b = 1): int { return $a + $b; }"
        "for ($i = 0; $i < 3; $i++) { if ($i === 2) break; echo $i; }";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *function;
    pc_ast *loop;
    if (program == NULL) {
        fprintf(stderr, "parser error: %s on line %u\n", pc_parser_error(&parser),
                pc_parser_error_line(&parser));
    }
    ASSERT_TRUE(program != NULL);
    ASSERT_EQ(2, program->as.list.count);
    function = program->as.list.items;
    loop = function->next;
    ASSERT_EQ(AST_FUNCTION, function->kind);
    ASSERT_EQ(2, function->as.function.parameter_count);
    ASSERT_EQ(AST_TYPE, function->as.function.parameters->as.parameter.type->kind);
    ASSERT_EQ(T_INT_TYPE,
              function->as.function.parameters->as.parameter.type
                  ->as.type_decl.name.type);
    ASSERT_EQ(T_INT_TYPE,
              function->as.function.return_type->as.type_decl.name.type);
    ASSERT_EQ(AST_FOR, loop->kind);
    ASSERT_EQ(AST_BLOCK, loop->as.for_stmt.body->kind);
    pc_arena_destroy(&arena);
}

TEST(arrays_calls_and_member_access_parse) {
    const char *source = "$x = foo([1, 'a' => 2, ...$rest])->value[0];";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *right;
    ASSERT_TRUE(program != NULL);
    right = program->as.list.items->as.expression.expression->as.binary.right;
    ASSERT_EQ(AST_INDEX, right->kind);
    ASSERT_EQ(AST_MEMBER, right->as.index.base->kind);
    ASSERT_EQ(AST_CALL, right->as.index.base->as.member.base->kind);
    ASSERT_EQ(AST_ARRAY,
              right->as.index.base->as.member.base->as.call.arguments->kind);
    pc_arena_destroy(&arena);
}

TEST(inline_html_and_short_echo_become_echo_nodes) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pphp_pool_init(parser_pool, sizeof(parser_pool));
    pc_arena_init(&arena, 2048U);
    pc_parser_init(&parser, &arena, "hello <?= 1 + 2 ?>!", 19U, 0);
    program = pc_parse_program(&parser);
    ASSERT_TRUE(program != NULL);
    ASSERT_EQ(3, program->as.list.count);
    ASSERT_EQ(AST_ECHO, program->as.list.items->kind);
    ASSERT_EQ(AST_ECHO, program->as.list.items->next->kind);
    ASSERT_EQ(AST_ECHO, program->as.list.items->next->next->kind);
    pc_arena_destroy(&arena);
}

TEST(parser_rejects_unsupported_reserved_syntax) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source("goto label;", &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "unsupported") != NULL);
    pc_arena_destroy(&arena);
}

TEST(parser_enforces_nesting_limit) {
    char source[256];
    size_t position = 0U;
    unsigned i;
    pc_arena arena;
    pc_parser parser;
    for (i = 0U; i < PPHP_PARSE_DEPTH_MAX + 1U; i++) {
        source[position++] = '(';
    }
    source[position++] = '1';
    for (i = 0U; i < PPHP_PARSE_DEPTH_MAX + 1U; i++) {
        source[position++] = ')';
    }
    source[position++] = ';';
    source[position] = '\0';
    ASSERT_TRUE(parse_source(source, &arena, &parser) == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "nesting") != NULL);
    pc_arena_destroy(&arena);
}

TEST(parser_reports_first_syntax_error_with_line) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source("$a = 1;\nif ($a { echo 2; }", &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_EQ(2, pc_parser_error_line(&parser));
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "expected ')'") != NULL);
    pc_arena_destroy(&arena);
}

TEST(classes_properties_methods_and_new_parse) {
    const char *source =
        "final class Counter extends Base {"
        " private int $value = 0;"
        " public function inc(int $by): int { $this->value += $by; return $this->value; }"
        "}"
        "$counter = new Counter(1); $counter->inc(2);";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *class_node;
    ASSERT_TRUE(program != NULL);
    class_node = program->as.list.items;
    ASSERT_EQ(AST_CLASS, class_node->kind);
    ASSERT_TRUE((class_node->as.class_decl.flags & PC_MOD_FINAL) != 0U);
    ASSERT_EQ(AST_PROPERTY, class_node->as.class_decl.members->kind);
    ASSERT_EQ(T_INT_TYPE,
              class_node->as.class_decl.members->as.property.type
                  ->as.type_decl.name.type);
    ASSERT_EQ(AST_FUNCTION, class_node->as.class_decl.members->next->kind);
    ASSERT_EQ(AST_ASSIGN, class_node->next->as.expression.expression->kind);
    ASSERT_EQ(AST_NEW,
              class_node->next->as.expression.expression->as.binary.right->kind);
    pc_arena_destroy(&arena);
}

TEST(try_catch_union_and_finally_build_exception_ast) {
    const char *source =
        "try { throw new RuntimeException('bad'); }"
        "catch (RuntimeException|Error $error) { echo $error; }"
        "finally { echo 'done'; }";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *try_node;
    pc_ast *catch_node;
    ASSERT_TRUE(program != NULL);
    try_node = program->as.list.items;
    ASSERT_EQ(AST_TRY, try_node->kind);
    ASSERT_EQ(AST_THROW, try_node->as.try_stmt.try_block->as.list.items->kind);
    catch_node = try_node->as.try_stmt.catches;
    ASSERT_EQ(AST_CATCH, catch_node->kind);
    ASSERT_TRUE(catch_node->as.catch_stmt.types->next != NULL);
    ASSERT_TRUE(catch_node->as.catch_stmt.types->next->next == NULL);
    ASSERT_EQ(AST_BLOCK, try_node->as.try_stmt.finally_block->kind);
    pc_arena_destroy(&arena);

    program = parse_source("try {}", &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "requires catch or finally") != NULL);
    pc_arena_destroy(&arena);
}

TEST(switch_cases_and_default_preserve_statement_groups) {
    const char *source =
        "switch ($value) {"
        " case 1: echo 'one'; break;"
        " case 2:"
        " case 3: echo 'several'; break;"
        " default: echo 'other';"
        "}";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *switch_node;
    pc_ast *case_node;
    ASSERT_TRUE(program != NULL);
    switch_node = program->as.list.items;
    ASSERT_EQ(AST_SWITCH, switch_node->kind);
    case_node = switch_node->as.switch_stmt.cases;
    ASSERT_EQ(AST_CASE, case_node->kind);
    ASSERT_EQ(2, case_node->as.case_stmt.body->as.list.count);
    ASSERT_EQ(0, case_node->next->as.case_stmt.body->as.list.count);
    ASSERT_TRUE(case_node->next->next->next->as.case_stmt.condition == NULL);
    pc_arena_destroy(&arena);

    program = parse_source("switch (1) { default: break; default: break; }",
                           &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "one default") != NULL);
    pc_arena_destroy(&arena);
}

TEST(match_parses_multiple_conditions_default_and_trailing_comma) {
    const char *source =
        "$result = match ($value) {"
        " 1, 2 => 'small',"
        " default => 'other',"
        "};";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *match_node;
    pc_ast *first_arm;
    ASSERT_TRUE(program != NULL);
    match_node = program->as.list.items->as.expression.expression->as.binary.right;
    ASSERT_EQ(AST_MATCH, match_node->kind);
    first_arm = match_node->as.match_expr.arms;
    ASSERT_EQ(AST_MATCH_ARM, first_arm->kind);
    ASSERT_TRUE(first_arm->as.match_arm.conditions->next != NULL);
    ASSERT_TRUE(first_arm->next->as.match_arm.conditions == NULL);
    pc_arena_destroy(&arena);

    program = parse_source(
        "match (1) { default => 1, default => 2, };", &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "one default") != NULL);
    pc_arena_destroy(&arena);
}

TEST(closures_and_arrow_functions_parse_capture_modes) {
    const char *source =
        "$first = function (int $x) use ($offset): int { return $x + $offset; };"
        "$second = static fn($x) => $x * 2;";
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(source, &arena, &parser);
    pc_ast *first;
    pc_ast *second;
    ASSERT_TRUE(program != NULL);
    first = program->as.list.items->as.expression.expression->as.binary.right;
    second = program->as.list.items->next->as.expression.expression->as.binary.right;
    ASSERT_EQ(AST_CLOSURE, first->kind);
    ASSERT_TRUE(!first->as.closure.is_arrow);
    ASSERT_EQ(AST_VARIABLE, first->as.closure.captures->kind);
    ASSERT_EQ(AST_BLOCK, first->as.closure.body->kind);
    ASSERT_EQ(T_INT_TYPE,
              first->as.closure.parameters->as.parameter.type
                  ->as.type_decl.name.type);
    ASSERT_EQ(T_INT_TYPE,
              first->as.closure.return_type->as.type_decl.name.type);
    ASSERT_EQ(AST_CLOSURE, second->kind);
    ASSERT_TRUE(second->as.closure.is_arrow);
    ASSERT_TRUE(second->as.closure.is_static);
    pc_arena_destroy(&arena);

    program = parse_source("function () use (&$value) {};", &arena, &parser);
    ASSERT_TRUE(program == NULL);
    ASSERT_TRUE(strstr(pc_parser_error(&parser), "by reference") != NULL);
    pc_arena_destroy(&arena);
}

TEST(nullable_and_union_types_are_preserved) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program = parse_source(
        "function typed(?int $first, string|bool|null $second): self {}",
        &arena, &parser);
    pc_ast *function;
    pc_ast *first_type;
    pc_ast *second_type;
    ASSERT_TRUE(program != NULL);
    function = program->as.list.items;
    first_type = function->as.function.parameters->as.parameter.type;
    second_type = function->as.function.parameters->next->as.parameter.type;
    ASSERT_EQ(T_NULL, first_type->as.type_decl.name.type);
    ASSERT_EQ(T_INT_TYPE, first_type->next->as.type_decl.name.type);
    ASSERT_TRUE(first_type->next->next == NULL);
    ASSERT_EQ(T_STRING_TYPE, second_type->as.type_decl.name.type);
    ASSERT_EQ(T_BOOL_TYPE, second_type->next->as.type_decl.name.type);
    ASSERT_EQ(T_NULL, second_type->next->next->as.type_decl.name.type);
    ASSERT_EQ(T_SELF, function->as.function.return_type->as.type_decl.name.type);
    pc_arena_destroy(&arena);
}

int main(void) {
    static const test_case tests[] = {
        {"operator precedence", operator_precedence_matches_php_8},
        {"assignment associativity", assignments_are_right_associative_and_validate_lvalues},
        {"control flow and functions", control_flow_and_functions_build_structured_ast},
        {"arrays and calls", arrays_calls_and_member_access_parse},
        {"inline HTML", inline_html_and_short_echo_become_echo_nodes},
        {"unsupported syntax", parser_rejects_unsupported_reserved_syntax},
        {"parser depth", parser_enforces_nesting_limit},
        {"syntax diagnostics", parser_reports_first_syntax_error_with_line},
        {"class syntax", classes_properties_methods_and_new_parse},
        {"exception syntax", try_catch_union_and_finally_build_exception_ast},
        {"switch syntax", switch_cases_and_default_preserve_statement_groups},
        {"match syntax", match_parses_multiple_conditions_default_and_trailing_comma},
        {"closure syntax", closures_and_arrow_functions_parse_capture_modes},
        {"declared types", nullable_and_union_types_are_preserved}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
