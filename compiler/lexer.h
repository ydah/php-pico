#ifndef PPHP_LEXER_H
#define PPHP_LEXER_H

#include <stddef.h>
#include <stdint.h>

typedef enum pc_token_type {
    T_EOF = 0,
    T_ERROR,
    T_INLINE_HTML,
    T_OPEN_TAG,
    T_OPEN_TAG_ECHO,
    T_CLOSE_TAG,
    T_IDENTIFIER,
    T_VARIABLE,
    T_INTEGER,
    T_FLOAT,
    T_SINGLE_QUOTED,
    T_DOUBLE_QUOTED,
    T_HEREDOC,
    T_NOWDOC,
    T_INTERP_START,
    T_INTERP_PART,
    T_INTERP_EXPR_START,
    T_INTERP_EXPR_END,
    T_INTERP_END,
    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_LBRACKET,
    T_RBRACKET,
    T_SEMICOLON,
    T_COLON,
    T_COMMA,
    T_DOT,
    T_QUESTION,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_PERCENT,
    T_AMP,
    T_PIPE,
    T_CARET,
    T_TILDE,
    T_BANG,
    T_EQUAL,
    T_LT,
    T_GT,
    T_PLUS_PLUS,
    T_MINUS_MINUS,
    T_POW,
    T_EQUAL_EQUAL,
    T_NOT_EQUAL,
    T_IDENTICAL,
    T_NOT_IDENTICAL,
    T_LT_EQUAL,
    T_GT_EQUAL,
    T_SPACESHIP,
    T_BOOL_AND,
    T_BOOL_OR,
    T_COALESCE,
    T_NULLSAFE_ARROW,
    T_ARROW,
    T_DOUBLE_ARROW,
    T_SCOPE,
    T_SHIFT_LEFT,
    T_SHIFT_RIGHT,
    T_PLUS_EQUAL,
    T_MINUS_EQUAL,
    T_STAR_EQUAL,
    T_SLASH_EQUAL,
    T_DOT_EQUAL,
    T_PERCENT_EQUAL,
    T_POW_EQUAL,
    T_AMP_EQUAL,
    T_PIPE_EQUAL,
    T_CARET_EQUAL,
    T_SHIFT_LEFT_EQUAL,
    T_SHIFT_RIGHT_EQUAL,
    T_COALESCE_EQUAL,
    T_ELLIPSIS,
    T_ABSTRACT,
    T_AND,
    T_ARRAY,
    T_AS,
    T_BREAK,
    T_CALLABLE,
    T_CASE,
    T_CATCH,
    T_CLASS,
    T_CLONE,
    T_CONST,
    T_CONTINUE,
    T_DECLARE,
    T_DEFAULT,
    T_DO,
    T_ECHO,
    T_ELSE,
    T_ELSEIF,
    T_EMPTY,
    T_ENDDECLARE,
    T_ENDFOR,
    T_ENDFOREACH,
    T_ENDIF,
    T_ENDSWITCH,
    T_ENDWHILE,
    T_ENUM,
    T_EXTENDS,
    T_FINAL,
    T_FINALLY,
    T_FN,
    T_FOR,
    T_FOREACH,
    T_FUNCTION,
    T_GLOBAL,
    T_GOTO,
    T_IF,
    T_IMPLEMENTS,
    T_INCLUDE,
    T_INCLUDE_ONCE,
    T_INSTANCEOF,
    T_INSTEADOF,
    T_INTERFACE,
    T_ISSET,
    T_LIST,
    T_MATCH,
    T_NAMESPACE,
    T_NEW,
    T_OR,
    T_PRINT,
    T_PRIVATE,
    T_PROTECTED,
    T_PUBLIC,
    T_READONLY,
    T_REQUIRE,
    T_REQUIRE_ONCE,
    T_RETURN,
    T_STATIC,
    T_SWITCH,
    T_THROW,
    T_TRAIT,
    T_TRY,
    T_UNSET,
    T_USE,
    T_VAR,
    T_WHILE,
    T_XOR,
    T_YIELD,
    T_TRUE,
    T_FALSE,
    T_NULL,
    T_INT_TYPE,
    T_FLOAT_TYPE,
    T_STRING_TYPE,
    T_BOOL_TYPE,
    T_VOID,
    T_MIXED,
    T_NEVER,
    T_SELF,
    T_PARENT
} pc_token_type;

typedef struct pc_token {
    pc_token_type type;
    const char *start;
    size_t length;
    uint32_t line;
    uint32_t column;
} pc_token;

typedef enum pc_lexer_mode {
    PC_LEX_NORMAL = 0,
    PC_LEX_INTERPOLATION,
    PC_LEX_INTERPOLATION_EXPR
} pc_lexer_mode;

typedef struct pc_lexer {
    const char *source;
    const char *current;
    const char *end;
    const char *interp_end;
    const char *error_start;
    uint32_t line;
    uint32_t column;
    uint32_t token_line;
    uint32_t token_column;
    unsigned interp_braces;
    uint32_t error_line;
    uint32_t error_column;
    int in_php;
    int repl;
    int pending_error;
    pc_lexer_mode mode;
    char error[160];
} pc_lexer;

void pc_lexer_init(pc_lexer *lexer, const char *source, size_t length, int repl);
pc_token pc_lexer_next(pc_lexer *lexer);
const char *pc_token_name(pc_token_type type);
const char *pc_lexer_error(const pc_lexer *lexer);

#endif
