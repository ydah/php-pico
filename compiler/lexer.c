#include "lexer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct keyword {
    const char *text;
    uint8_t length;
    pc_token_type type;
} keyword;

static const keyword keywords[] = {
    {"abstract", 8, T_ABSTRACT}, {"and", 3, T_AND},
    {"array", 5, T_ARRAY}, {"as", 2, T_AS}, {"break", 5, T_BREAK},
    {"bool", 4, T_BOOL_TYPE}, {"callable", 8, T_CALLABLE},
    {"case", 4, T_CASE}, {"catch", 5, T_CATCH}, {"class", 5, T_CLASS},
    {"clone", 5, T_CLONE}, {"const", 5, T_CONST},
    {"continue", 8, T_CONTINUE}, {"declare", 7, T_DECLARE},
    {"default", 7, T_DEFAULT}, {"do", 2, T_DO}, {"echo", 4, T_ECHO},
    {"else", 4, T_ELSE}, {"elseif", 6, T_ELSEIF}, {"empty", 5, T_EMPTY},
    {"enddeclare", 10, T_ENDDECLARE}, {"endfor", 6, T_ENDFOR},
    {"endforeach", 10, T_ENDFOREACH}, {"endif", 5, T_ENDIF},
    {"endswitch", 9, T_ENDSWITCH}, {"endwhile", 8, T_ENDWHILE},
    {"enum", 4, T_ENUM}, {"extends", 7, T_EXTENDS}, {"false", 5, T_FALSE},
    {"final", 5, T_FINAL}, {"finally", 7, T_FINALLY}, {"float", 5, T_FLOAT_TYPE},
    {"fn", 2, T_FN}, {"for", 3, T_FOR}, {"foreach", 7, T_FOREACH},
    {"function", 8, T_FUNCTION}, {"global", 6, T_GLOBAL}, {"goto", 4, T_GOTO},
    {"if", 2, T_IF}, {"implements", 10, T_IMPLEMENTS},
    {"include", 7, T_INCLUDE}, {"include_once", 12, T_INCLUDE_ONCE},
    {"instanceof", 10, T_INSTANCEOF}, {"insteadof", 9, T_INSTEADOF},
    {"int", 3, T_INT_TYPE}, {"interface", 9, T_INTERFACE},
    {"isset", 5, T_ISSET}, {"list", 4, T_LIST}, {"match", 5, T_MATCH},
    {"mixed", 5, T_MIXED}, {"namespace", 9, T_NAMESPACE}, {"never", 5, T_NEVER},
    {"new", 3, T_NEW}, {"null", 4, T_NULL}, {"or", 2, T_OR},
    {"parent", 6, T_PARENT}, {"print", 5, T_PRINT}, {"private", 7, T_PRIVATE},
    {"protected", 9, T_PROTECTED}, {"public", 6, T_PUBLIC},
    {"readonly", 8, T_READONLY}, {"require", 7, T_REQUIRE},
    {"require_once", 12, T_REQUIRE_ONCE}, {"return", 6, T_RETURN},
    {"self", 4, T_SELF}, {"static", 6, T_STATIC}, {"string", 6, T_STRING_TYPE},
    {"switch", 6, T_SWITCH}, {"throw", 5, T_THROW}, {"trait", 5, T_TRAIT},
    {"true", 4, T_TRUE}, {"try", 3, T_TRY}, {"unset", 5, T_UNSET},
    {"use", 3, T_USE}, {"var", 3, T_VAR}, {"void", 4, T_VOID},
    {"while", 5, T_WHILE}, {"xor", 3, T_XOR}, {"yield", 5, T_YIELD}
};

static int at_end(const pc_lexer *lexer) {
    return lexer->current >= lexer->end;
}

static char peek(const pc_lexer *lexer) {
    return at_end(lexer) ? '\0' : *lexer->current;
}

static char peek_next(const pc_lexer *lexer) {
    return lexer->current + 1 >= lexer->end ? '\0' : lexer->current[1];
}

static char advance(pc_lexer *lexer) {
    char value = *lexer->current++;
    if (value == '\n') {
        lexer->line++;
        lexer->column = 1U;
    } else {
        lexer->column++;
    }
    return value;
}

static int match_char(pc_lexer *lexer, char expected) {
    if (at_end(lexer) || *lexer->current != expected) {
        return 0;
    }
    advance(lexer);
    return 1;
}

static pc_token make_token(const pc_lexer *lexer, pc_token_type type,
                           const char *start) {
    pc_token token;
    token.type = type;
    token.start = start;
    token.length = (size_t)(lexer->current - start);
    token.line = lexer->token_line;
    token.column = lexer->token_column;
    return token;
}

static pc_token error_token(pc_lexer *lexer, const char *start,
                            const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    (void)vsnprintf(lexer->error, sizeof(lexer->error), format, arguments);
    va_end(arguments);
    return make_token(lexer, T_ERROR, start);
}

static int is_identifier_start(unsigned char value) {
    return value == '_' || (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || value >= 0x80U;
}

static int is_identifier_part(unsigned char value) {
    return is_identifier_start(value) || (value >= '0' && value <= '9');
}

static int ascii_equal_ci(const char *left, const char *right, size_t length) {
    size_t i;
    for (i = 0U; i < length; i++) {
        unsigned char a = (unsigned char)left[i];
        unsigned char b = (unsigned char)right[i];
        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char)(a + ('a' - 'A'));
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static pc_token_type identifier_type(const char *start, size_t length) {
    size_t i;
    for (i = 0U; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (keywords[i].length == length &&
            ascii_equal_ci(start, keywords[i].text, length)) {
            return keywords[i].type;
        }
    }
    return T_IDENTIFIER;
}

static pc_token scan_identifier(pc_lexer *lexer, const char *start) {
    while (!at_end(lexer) && is_identifier_part((unsigned char)peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, identifier_type(start, (size_t)(lexer->current - start)), start);
}

static int is_digit_for_base(char value, unsigned base) {
    if (value >= '0' && value <= '9') {
        return (unsigned)(value - '0') < base;
    }
    if (value >= 'a' && value <= 'f') {
        return base == 16U;
    }
    if (value >= 'A' && value <= 'F') {
        return base == 16U;
    }
    return 0;
}

static size_t scan_digits(pc_lexer *lexer, unsigned base) {
    size_t count = 0U;
    int underscore = 0;
    while (!at_end(lexer)) {
        char value = peek(lexer);
        if (is_digit_for_base(value, base)) {
            advance(lexer);
            count++;
            underscore = 0;
        } else if (value == '_' && !underscore && lexer->current > lexer->source &&
                   is_digit_for_base(lexer->current[-1], base) &&
                   lexer->current + 1 < lexer->end &&
                   is_digit_for_base(lexer->current[1], base)) {
            advance(lexer);
            underscore = 1;
        } else {
            break;
        }
    }
    return count;
}

static pc_token scan_number(pc_lexer *lexer, const char *start, int leading_dot) {
    int is_float = leading_dot;
    if (leading_dot) {
        (void)scan_digits(lexer, 10U);
    } else if (*start == '0' && !at_end(lexer)) {
        char prefix = peek(lexer);
        unsigned base = 0U;
        if (prefix == 'x' || prefix == 'X') {
            base = 16U;
        } else if (prefix == 'b' || prefix == 'B') {
            base = 2U;
        } else if (prefix == 'o' || prefix == 'O') {
            base = 8U;
        }
        if (base != 0U) {
            advance(lexer);
            if (scan_digits(lexer, base) == 0U) {
                return error_token(lexer, start, "expected base-%u digit after numeric prefix", base);
            }
            return make_token(lexer, T_INTEGER, start);
        }
        (void)scan_digits(lexer, 10U);
    } else {
        (void)scan_digits(lexer, 10U);
    }
    if (!at_end(lexer) && peek(lexer) == '.' && peek_next(lexer) != '.') {
        is_float = 1;
        advance(lexer);
        (void)scan_digits(lexer, 10U);
    }
    if (!at_end(lexer) && (peek(lexer) == 'e' || peek(lexer) == 'E')) {
        const char *exponent = lexer->current;
        uint32_t exponent_column = lexer->column;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        if (scan_digits(lexer, 10U) == 0U) {
            lexer->current = exponent;
            lexer->column = exponent_column;
        } else {
            is_float = 1;
        }
    }
    return make_token(lexer, is_float ? T_FLOAT : T_INTEGER, start);
}

static const char *find_double_quote_end(const pc_lexer *lexer) {
    const char *cursor = lexer->current;
    while (cursor < lexer->end) {
        if (*cursor == '\\' && cursor + 1 < lexer->end) {
            cursor += 2;
            continue;
        }
        if (*cursor == '"') {
            return cursor;
        }
        cursor++;
    }
    return NULL;
}

static int contains_interpolation(const char *start, const char *end) {
    const char *cursor = start;
    while (cursor < end) {
        if (*cursor == '\\' && cursor + 1 < end) {
            cursor += 2;
            continue;
        }
        if (*cursor == '$' && cursor + 1 < end &&
            is_identifier_start((unsigned char)cursor[1])) {
            return 1;
        }
        if (*cursor == '{' && cursor + 2 < end && cursor[1] == '$' &&
            is_identifier_start((unsigned char)cursor[2])) {
            return 1;
        }
        cursor++;
    }
    return 0;
}

static pc_token scan_quoted(pc_lexer *lexer, const char *start, char quote) {
    if (quote == '"') {
        const char *end = find_double_quote_end(lexer);
        if (end == NULL) {
            while (!at_end(lexer)) {
                advance(lexer);
            }
            return error_token(lexer, start, "unterminated double-quoted string");
        }
        if (contains_interpolation(lexer->current, end)) {
            lexer->interp_end = end;
            lexer->mode = PC_LEX_INTERPOLATION;
            return make_token(lexer, T_INTERP_START, start);
        }
    }
    while (!at_end(lexer)) {
        char value = advance(lexer);
        if (value == quote) {
            return make_token(lexer,
                              quote == '\'' ? T_SINGLE_QUOTED : T_DOUBLE_QUOTED,
                              start);
        }
        if (value == '\\' && !at_end(lexer)) {
            advance(lexer);
        }
    }
    return error_token(lexer, start, "unterminated %s-quoted string",
                       quote == '\'' ? "single" : "double");
}

static int is_line_end(char value) {
    return value == '\n' || value == '\r' || value == '\0';
}

static int starts_open_tag(const pc_lexer *lexer) {
    if (lexer->end - lexer->current >= 3 &&
        memcmp(lexer->current, "<?=", 3U) == 0) {
        return 1;
    }
    if (lexer->end - lexer->current < 5 ||
        memcmp(lexer->current, "<?php", 5U) != 0) {
        return 0;
    }
    return lexer->current + 5 == lexer->end ||
           lexer->current[5] == ' ' || lexer->current[5] == '\t' ||
           lexer->current[5] == '\r' || lexer->current[5] == '\n';
}

static pc_token scan_heredoc(pc_lexer *lexer, const char *start) {
    char label[64];
    size_t label_length = 0U;
    char quote = '\0';
    int nowdoc = 0;

    if (peek(lexer) == '\'' || peek(lexer) == '"') {
        quote = advance(lexer);
        nowdoc = quote == '\'';
    }
    while (!at_end(lexer) && is_identifier_part((unsigned char)peek(lexer))) {
        if (label_length + 1U >= sizeof(label)) {
            return error_token(lexer, start, "heredoc label is too long");
        }
        label[label_length++] = advance(lexer);
    }
    if (label_length == 0U || (quote != '\0' && !match_char(lexer, quote))) {
        return error_token(lexer, start, "invalid heredoc label");
    }
    if (peek(lexer) == '\r') {
        advance(lexer);
    }
    if (!match_char(lexer, '\n')) {
        return error_token(lexer, start, "expected newline after heredoc label");
    }
    while (!at_end(lexer)) {
        const char *line_start = lexer->current;
        const char *candidate;
        while (peek(lexer) == ' ' || peek(lexer) == '\t') {
            advance(lexer);
        }
        candidate = lexer->current;
        if ((size_t)(lexer->end - candidate) >= label_length &&
            memcmp(candidate, label, label_length) == 0 &&
            (candidate + label_length == lexer->end ||
             candidate[label_length] == ';' || is_line_end(candidate[label_length]))) {
            while (!at_end(lexer) && !is_line_end(peek(lexer))) {
                advance(lexer);
            }
            if (peek(lexer) == '\r') {
                advance(lexer);
            }
            if (peek(lexer) == '\n') {
                advance(lexer);
            }
            return make_token(lexer, nowdoc ? T_NOWDOC : T_HEREDOC, start);
        }
        lexer->current = line_start;
        lexer->column = 1U;
        while (!at_end(lexer) && peek(lexer) != '\n') {
            advance(lexer);
        }
        if (!at_end(lexer)) {
            advance(lexer);
        }
    }
    return error_token(lexer, start, "unterminated heredoc %.*s",
                       (int)label_length, label);
}

static void skip_space_and_comments(pc_lexer *lexer) {
    for (;;) {
        if (at_end(lexer)) {
            return;
        }
        if (peek(lexer) == ' ' || peek(lexer) == '\t' || peek(lexer) == '\r' ||
            peek(lexer) == '\n') {
            advance(lexer);
            continue;
        }
        if (peek(lexer) == '#' || (peek(lexer) == '/' && peek_next(lexer) == '/')) {
            while (!at_end(lexer) && peek(lexer) != '\n' &&
                   !(peek(lexer) == '?' && peek_next(lexer) == '>')) {
                advance(lexer);
            }
            continue;
        }
        if (peek(lexer) == '/' && peek_next(lexer) == '*') {
            const char *comment_start = lexer->current;
            uint32_t comment_line = lexer->line;
            uint32_t comment_column = lexer->column;
            advance(lexer);
            advance(lexer);
            while (!at_end(lexer) && !(peek(lexer) == '*' && peek_next(lexer) == '/')) {
                advance(lexer);
            }
            if (at_end(lexer)) {
                lexer->error_start = comment_start;
                lexer->error_line = comment_line;
                lexer->error_column = comment_column;
                lexer->pending_error = 1;
                (void)snprintf(lexer->error, sizeof(lexer->error),
                               "unterminated block comment");
                return;
            }
            advance(lexer);
            advance(lexer);
            continue;
        }
        return;
    }
}

static pc_token scan_inline_html(pc_lexer *lexer) {
    const char *start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;
    while (!at_end(lexer)) {
        if (peek(lexer) == '<' && starts_open_tag(lexer)) {
            break;
        }
        advance(lexer);
    }
    return make_token(lexer, T_INLINE_HTML, start);
}

static pc_token lex_interpolation(pc_lexer *lexer) {
    const char *start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;
    if (lexer->current >= lexer->interp_end) {
        advance(lexer);
        lexer->mode = PC_LEX_NORMAL;
        lexer->interp_end = NULL;
        return make_token(lexer, T_INTERP_END, start);
    }
    if (peek(lexer) == '$' && lexer->current + 1 < lexer->interp_end &&
        is_identifier_start((unsigned char)lexer->current[1])) {
        advance(lexer);
        while (lexer->current < lexer->interp_end &&
               is_identifier_part((unsigned char)peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, T_VARIABLE, start);
    }
    if (peek(lexer) == '{' && lexer->current + 2 < lexer->interp_end &&
        lexer->current[1] == '$' &&
        is_identifier_start((unsigned char)lexer->current[2])) {
        advance(lexer);
        lexer->mode = PC_LEX_INTERPOLATION_EXPR;
        lexer->interp_braces = 0U;
        return make_token(lexer, T_INTERP_EXPR_START, start);
    }
    while (lexer->current < lexer->interp_end) {
        if (peek(lexer) == '\\' && lexer->current + 1 < lexer->interp_end) {
            advance(lexer);
            advance(lexer);
            continue;
        }
        if ((peek(lexer) == '$' && lexer->current + 1 < lexer->interp_end &&
             is_identifier_start((unsigned char)lexer->current[1])) ||
            (peek(lexer) == '{' && lexer->current + 2 < lexer->interp_end &&
             lexer->current[1] == '$')) {
            break;
        }
        advance(lexer);
    }
    return make_token(lexer, T_INTERP_PART, start);
}

static pc_token scan_operator(pc_lexer *lexer, const char *start, char value) {
    switch (value) {
        case '(' : return make_token(lexer, T_LPAREN, start);
        case ')' : return make_token(lexer, T_RPAREN, start);
        case '{' : return make_token(lexer, T_LBRACE, start);
        case '}' : return make_token(lexer, T_RBRACE, start);
        case '[' : return make_token(lexer, T_LBRACKET, start);
        case ']' : return make_token(lexer, T_RBRACKET, start);
        case ';' : return make_token(lexer, T_SEMICOLON, start);
        case ':' : return make_token(lexer, match_char(lexer, ':') ? T_SCOPE : T_COLON, start);
        case ',' : return make_token(lexer, T_COMMA, start);
        case '~' : return make_token(lexer, T_TILDE, start);
        case '.':
            if (match_char(lexer, '.')) {
                if (match_char(lexer, '.')) return make_token(lexer, T_ELLIPSIS, start);
                return error_token(lexer, start, "unexpected '..'");
            }
            return make_token(lexer, match_char(lexer, '=') ? T_DOT_EQUAL : T_DOT, start);
        case '+':
            if (match_char(lexer, '+')) return make_token(lexer, T_PLUS_PLUS, start);
            return make_token(lexer, match_char(lexer, '=') ? T_PLUS_EQUAL : T_PLUS, start);
        case '-':
            if (match_char(lexer, '-')) return make_token(lexer, T_MINUS_MINUS, start);
            if (match_char(lexer, '>')) return make_token(lexer, T_ARROW, start);
            return make_token(lexer, match_char(lexer, '=') ? T_MINUS_EQUAL : T_MINUS, start);
        case '*':
            if (match_char(lexer, '*')) {
                return make_token(lexer, match_char(lexer, '=') ? T_POW_EQUAL : T_POW, start);
            }
            return make_token(lexer, match_char(lexer, '=') ? T_STAR_EQUAL : T_STAR, start);
        case '/': return make_token(lexer, match_char(lexer, '=') ? T_SLASH_EQUAL : T_SLASH, start);
        case '%': return make_token(lexer, match_char(lexer, '=') ? T_PERCENT_EQUAL : T_PERCENT, start);
        case '^': return make_token(lexer, match_char(lexer, '=') ? T_CARET_EQUAL : T_CARET, start);
        case '&':
            if (match_char(lexer, '&')) return make_token(lexer, T_BOOL_AND, start);
            return make_token(lexer, match_char(lexer, '=') ? T_AMP_EQUAL : T_AMP, start);
        case '|':
            if (match_char(lexer, '|')) return make_token(lexer, T_BOOL_OR, start);
            return make_token(lexer, match_char(lexer, '=') ? T_PIPE_EQUAL : T_PIPE, start);
        case '!':
            if (match_char(lexer, '=')) {
                return make_token(lexer, match_char(lexer, '=') ? T_NOT_IDENTICAL : T_NOT_EQUAL, start);
            }
            return make_token(lexer, T_BANG, start);
        case '=':
            if (match_char(lexer, '=')) {
                return make_token(lexer, match_char(lexer, '=') ? T_IDENTICAL : T_EQUAL_EQUAL, start);
            }
            return make_token(lexer, match_char(lexer, '>') ? T_DOUBLE_ARROW : T_EQUAL, start);
        case '<':
            if (match_char(lexer, '=')) {
                if (match_char(lexer, '>')) return make_token(lexer, T_SPACESHIP, start);
                return make_token(lexer, T_LT_EQUAL, start);
            }
            if (match_char(lexer, '<')) {
                return make_token(lexer, match_char(lexer, '=') ? T_SHIFT_LEFT_EQUAL : T_SHIFT_LEFT, start);
            }
            if (match_char(lexer, '>')) return make_token(lexer, T_NOT_EQUAL, start);
            return make_token(lexer, T_LT, start);
        case '>':
            if (match_char(lexer, '>')) {
                return make_token(lexer, match_char(lexer, '=') ? T_SHIFT_RIGHT_EQUAL : T_SHIFT_RIGHT, start);
            }
            return make_token(lexer, match_char(lexer, '=') ? T_GT_EQUAL : T_GT, start);
        case '?':
            if (match_char(lexer, '?')) {
                return make_token(lexer, match_char(lexer, '=') ? T_COALESCE_EQUAL : T_COALESCE, start);
            }
            if (lexer->end - lexer->current >= 2 && lexer->current[0] == '-' &&
                lexer->current[1] == '>') {
                advance(lexer);
                advance(lexer);
                return make_token(lexer, T_NULLSAFE_ARROW, start);
            }
            return make_token(lexer, T_QUESTION, start);
        default:
            return error_token(lexer, start, "unexpected byte 0x%02x", (unsigned char)value);
    }
}

void pc_lexer_init(pc_lexer *lexer, const char *source, size_t length, int repl) {
    memset(lexer, 0, sizeof(*lexer));
    lexer->source = source;
    lexer->current = source;
    lexer->end = source + length;
    lexer->line = 1U;
    lexer->column = 1U;
    lexer->repl = repl != 0;
    lexer->in_php = repl != 0;
}

pc_token pc_lexer_next(pc_lexer *lexer) {
    const char *start;
    char value;

    if (lexer->mode == PC_LEX_INTERPOLATION) {
        return lex_interpolation(lexer);
    }
    if (!lexer->in_php && !at_end(lexer)) {
        if (!(peek(lexer) == '<' && starts_open_tag(lexer))) {
            return scan_inline_html(lexer);
        }
    }
    skip_space_and_comments(lexer);
    if (lexer->pending_error) {
        pc_token token;
        lexer->pending_error = 0;
        token.type = T_ERROR;
        token.start = lexer->error_start;
        token.length = (size_t)(lexer->current - lexer->error_start);
        token.line = lexer->error_line;
        token.column = lexer->error_column;
        return token;
    }
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;
    start = lexer->current;
    if (at_end(lexer)) {
        return make_token(lexer, T_EOF, start);
    }
    if (!lexer->in_php && starts_open_tag(lexer) &&
        lexer->end - lexer->current >= 5 &&
        memcmp(lexer->current, "<?php", 5U) == 0) {
        lexer->current += 5;
        lexer->column += 5U;
        lexer->in_php = 1;
        return make_token(lexer, T_OPEN_TAG, start);
    }
    if (!lexer->in_php && lexer->end - lexer->current >= 3 &&
        memcmp(lexer->current, "<?=", 3U) == 0) {
        lexer->current += 3;
        lexer->column += 3U;
        lexer->in_php = 1;
        return make_token(lexer, T_OPEN_TAG_ECHO, start);
    }
    if (lexer->in_php && peek(lexer) == '?' && peek_next(lexer) == '>') {
        advance(lexer);
        advance(lexer);
        lexer->in_php = lexer->repl;
        return make_token(lexer, T_CLOSE_TAG, start);
    }
    value = advance(lexer);
    if (lexer->mode == PC_LEX_INTERPOLATION_EXPR) {
        if (value == '}' && lexer->interp_braces == 0U) {
            lexer->mode = PC_LEX_INTERPOLATION;
            return make_token(lexer, T_INTERP_EXPR_END, start);
        }
        if (value == '{') {
            lexer->interp_braces++;
        } else if (value == '}' && lexer->interp_braces != 0U) {
            lexer->interp_braces--;
        }
    }
    if (is_identifier_start((unsigned char)value)) {
        return scan_identifier(lexer, start);
    }
    if (value >= '0' && value <= '9') {
        return scan_number(lexer, start, 0);
    }
    if (value == '.' && peek(lexer) >= '0' && peek(lexer) <= '9') {
        return scan_number(lexer, start, 1);
    }
    if (value == '$') {
        if (!is_identifier_start((unsigned char)peek(lexer))) {
            return error_token(lexer, start, "expected identifier after '$'");
        }
        while (!at_end(lexer) && is_identifier_part((unsigned char)peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, T_VARIABLE, start);
    }
    if (value == '\'' || value == '"') {
        return scan_quoted(lexer, start, value);
    }
    if (value == '<' && lexer->end - lexer->current >= 2 &&
        lexer->current[0] == '<' && lexer->current[1] == '<') {
        advance(lexer);
        advance(lexer);
        return scan_heredoc(lexer, start);
    }
    return scan_operator(lexer, start, value);
}

const char *pc_lexer_error(const pc_lexer *lexer) {
    return lexer->error;
}

const char *pc_token_name(pc_token_type type) {
    static const char *const names[] = {
        "EOF", "ERROR", "INLINE_HTML", "OPEN_TAG", "OPEN_TAG_ECHO", "CLOSE_TAG",
        "IDENTIFIER", "VARIABLE", "INTEGER", "FLOAT", "SINGLE_QUOTED", "DOUBLE_QUOTED",
        "HEREDOC", "NOWDOC", "INTERP_START", "INTERP_PART", "INTERP_EXPR_START",
        "INTERP_EXPR_END", "INTERP_END", "LPAREN", "RPAREN", "LBRACE", "RBRACE",
        "LBRACKET", "RBRACKET", "SEMICOLON", "COLON", "COMMA", "DOT", "QUESTION",
        "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "AMP", "PIPE", "CARET", "TILDE",
        "BANG", "EQUAL", "LT", "GT", "PLUS_PLUS", "MINUS_MINUS", "POW", "EQUAL_EQUAL",
        "NOT_EQUAL", "IDENTICAL", "NOT_IDENTICAL", "LT_EQUAL", "GT_EQUAL", "SPACESHIP",
        "BOOL_AND", "BOOL_OR", "COALESCE", "NULLSAFE_ARROW", "ARROW", "DOUBLE_ARROW",
        "SCOPE", "SHIFT_LEFT", "SHIFT_RIGHT", "PLUS_EQUAL", "MINUS_EQUAL", "STAR_EQUAL",
        "SLASH_EQUAL", "DOT_EQUAL", "PERCENT_EQUAL", "POW_EQUAL", "AMP_EQUAL", "PIPE_EQUAL",
        "CARET_EQUAL", "SHIFT_LEFT_EQUAL", "SHIFT_RIGHT_EQUAL", "COALESCE_EQUAL", "ELLIPSIS",
        "ABSTRACT", "AND", "ARRAY", "AS", "BREAK", "CALLABLE", "CASE", "CATCH", "CLASS",
        "CLONE", "CONST", "CONTINUE", "DECLARE", "DEFAULT", "DO", "ECHO", "ELSE", "ELSEIF",
        "EMPTY", "ENDDECLARE", "ENDFOR", "ENDFOREACH", "ENDIF", "ENDSWITCH", "ENDWHILE",
        "ENUM", "EXTENDS", "FINAL", "FINALLY", "FN", "FOR", "FOREACH", "FUNCTION", "GLOBAL",
        "GOTO", "IF", "IMPLEMENTS", "INCLUDE", "INCLUDE_ONCE", "INSTANCEOF", "INSTEADOF",
        "INTERFACE", "ISSET", "LIST", "MATCH", "NAMESPACE", "NEW", "OR", "PRINT", "PRIVATE",
        "PROTECTED", "PUBLIC", "READONLY", "REQUIRE", "REQUIRE_ONCE", "RETURN", "STATIC",
        "SWITCH", "THROW", "TRAIT", "TRY", "UNSET", "USE", "VAR", "WHILE", "XOR", "YIELD",
        "TRUE", "FALSE", "NULL", "INT_TYPE", "FLOAT_TYPE", "STRING_TYPE", "BOOL_TYPE", "VOID",
        "MIXED", "NEVER", "SELF", "PARENT"
    };
    if ((size_t)type >= sizeof(names) / sizeof(names[0])) {
        return "UNKNOWN";
    }
    return names[type];
}
