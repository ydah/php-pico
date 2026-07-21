#include "test.h"

#include "lexer.h"

static void assert_sequence(const char *source, int repl,
                            const pc_token_type *expected, size_t count) {
    pc_lexer lexer;
    size_t i;
    pc_lexer_init(&lexer, source, strlen(source), repl);
    for (i = 0U; i < count; i++) {
        pc_token token = pc_lexer_next(&lexer);
        if (token.type != expected[i]) {
            fprintf(stderr, "token %zu: expected %s, got %s (%.*s)\n", i,
                    pc_token_name(expected[i]), pc_token_name(token.type),
                    (int)token.length, token.start);
            test_failures++;
            return;
        }
    }
}

TEST(tags_and_inline_html_are_preserved) {
    static const pc_token_type expected[] = {
        T_INLINE_HTML, T_OPEN_TAG_ECHO, T_INTEGER, T_CLOSE_TAG,
        T_INLINE_HTML, T_EOF
    };
    assert_sequence("hello <?= 42 ?> world", 0, expected,
                    sizeof(expected) / sizeof(expected[0]));
}

TEST(comments_and_positions_are_tracked) {
    pc_lexer lexer;
    pc_token token;
    const char *source = "<?php\n/* two\nlines */\n// x\necho # y\n 1;";
    pc_lexer_init(&lexer, source, strlen(source), 0);
    token = pc_lexer_next(&lexer);
    ASSERT_EQ(T_OPEN_TAG, token.type);
    token = pc_lexer_next(&lexer);
    ASSERT_EQ(T_ECHO, token.type);
    ASSERT_EQ(5, token.line);
    ASSERT_EQ(1, token.column);
    token = pc_lexer_next(&lexer);
    ASSERT_EQ(T_INTEGER, token.type);
    ASSERT_EQ(6, token.line);
    ASSERT_EQ(2, token.column);
}

TEST(numbers_cover_php_literal_forms) {
    static const pc_token_type expected[] = {
        T_INTEGER, T_INTEGER, T_INTEGER, T_INTEGER, T_INTEGER,
        T_FLOAT, T_FLOAT, T_FLOAT, T_EOF
    };
    assert_sequence("123 0x1f 0b101 0o17 1_000 .5 1.5 2e-3", 1,
                    expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(invalid_numeric_prefix_is_an_error) {
    pc_lexer lexer;
    pc_token token;
    pc_lexer_init(&lexer, "0x;", 3U, 1);
    token = pc_lexer_next(&lexer);
    ASSERT_EQ(T_ERROR, token.type);
    ASSERT_TRUE(strstr(pc_lexer_error(&lexer), "base-16") != NULL);
}

TEST(all_multi_byte_operators_use_longest_match) {
    static const pc_token_type expected[] = {
        T_POW_EQUAL, T_IDENTICAL, T_NOT_IDENTICAL, T_SPACESHIP,
        T_SHIFT_LEFT_EQUAL, T_SHIFT_RIGHT_EQUAL, T_COALESCE_EQUAL,
        T_NULLSAFE_ARROW, T_DOUBLE_ARROW, T_ELLIPSIS, T_EOF
    };
    assert_sequence("**= === !== <=> <<= >>= ?" "?= ?-> => ...", 1,
                    expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(keywords_are_case_insensitive_but_variables_are_not) {
    static const pc_token_type expected[] = {
        T_FUNCTION, T_MATCH, T_READONLY, T_TRUE, T_VARIABLE, T_VARIABLE, T_EOF
    };
    assert_sequence("FuNcTiOn MATCH readonly TRUE $Name $name", 1,
                    expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(strings_and_interpolation_are_tokenized) {
    static const pc_token_type expected[] = {
        T_SINGLE_QUOTED, T_DOUBLE_QUOTED, T_INTERP_START, T_INTERP_PART,
        T_VARIABLE, T_INTERP_PART, T_VARIABLE, T_INTERP_PART,
        T_INTERP_EXPR_START, T_VARIABLE, T_PLUS, T_INTEGER,
        T_INTERP_EXPR_END, T_INTERP_END, T_EOF
    };
    assert_sequence("'a' \"b\" \"hello $name { $x } {$x+1}\"", 1,
                    expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(heredoc_and_nowdoc_terminate_on_indented_label) {
    static const pc_token_type expected[] = {
        T_HEREDOC, T_SEMICOLON, T_NOWDOC, T_EOF
    };
    const char *source = "<<<TXT\nhello\n  TXT;\n<<<'RAW'\n$x\nRAW\n";
    assert_sequence(source, 1, expected, sizeof(expected) / sizeof(expected[0]));
}

TEST(unterminated_strings_report_an_error) {
    pc_lexer lexer;
    pc_token token;
    pc_lexer_init(&lexer, "\"missing", 8U, 1);
    token = pc_lexer_next(&lexer);
    ASSERT_EQ(T_ERROR, token.type);
    ASSERT_TRUE(strstr(pc_lexer_error(&lexer), "unterminated") != NULL);
}

TEST(unterminated_comments_report_an_error) {
    pc_lexer lexer;
    pc_token token;
    pc_lexer_init(&lexer, "/* missing", 10U, 1);
    token = pc_lexer_next(&lexer);
    ASSERT_EQ(T_ERROR, token.type);
    ASSERT_EQ(1, token.line);
    ASSERT_TRUE(strstr(pc_lexer_error(&lexer), "comment") != NULL);
}

TEST(lexer_fuzz_inputs_always_make_progress) {
    char bytes[64];
    uint32_t random = UINT32_C(0x51ed270b);
    int iteration;
    for (iteration = 0; iteration < 10000; iteration++) {
        pc_lexer lexer;
        size_t length;
        size_t i;
        size_t tokens = 0U;
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        length = (random >> 24U) & 63U;
        for (i = 0U; i < length; i++) {
            random = random * UINT32_C(1664525) + UINT32_C(1013904223);
            bytes[i] = (char)(random >> 24U);
        }
        pc_lexer_init(&lexer, bytes, length, 1);
        for (;;) {
            pc_token token = pc_lexer_next(&lexer);
            tokens++;
            ASSERT_TRUE(tokens < 512U);
            if (token.type == T_EOF || token.type == T_ERROR) {
                break;
            }
        }
    }
}

int main(void) {
    static const test_case tests[] = {
        {"PHP tags and inline HTML", tags_and_inline_html_are_preserved},
        {"comment and source positions", comments_and_positions_are_tracked},
        {"numeric literals", numbers_cover_php_literal_forms},
        {"numeric prefix errors", invalid_numeric_prefix_is_an_error},
        {"operators", all_multi_byte_operators_use_longest_match},
        {"keywords", keywords_are_case_insensitive_but_variables_are_not},
        {"interpolated strings", strings_and_interpolation_are_tokenized},
        {"heredoc", heredoc_and_nowdoc_terminate_on_indented_label},
        {"unterminated strings", unterminated_strings_report_an_error},
        {"unterminated comments", unterminated_comments_report_an_error},
        {"fuzz input progress", lexer_fuzz_inputs_always_make_progress}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
