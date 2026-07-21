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

int main(void) {
    static const test_case tests[] = {
        {"operator precedence", operator_precedence_matches_php_8},
        {"assignment associativity", assignments_are_right_associative_and_validate_lvalues},
        {"control flow and functions", control_flow_and_functions_build_structured_ast},
        {"arrays and calls", arrays_calls_and_member_access_parse},
        {"inline HTML", inline_html_and_short_echo_become_echo_nodes},
        {"unsupported syntax", parser_rejects_unsupported_reserved_syntax},
        {"parser depth", parser_enforces_nesting_limit},
        {"syntax diagnostics", parser_reports_first_syntax_error_with_line}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
