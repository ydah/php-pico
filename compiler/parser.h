#ifndef PPHP_PARSER_H
#define PPHP_PARSER_H

#include <stddef.h>

#include "ast.h"

typedef struct pc_parser {
    pc_lexer lexer;
    pc_token current;
    pc_token previous;
    pc_arena *arena;
    unsigned depth;
    int failed;
    char error[192];
} pc_parser;

void pc_parser_init(pc_parser *parser, pc_arena *arena, const char *source,
                    size_t length, int repl);
pc_ast *pc_parse_program(pc_parser *parser);
const char *pc_parser_error(const pc_parser *parser);
uint32_t pc_parser_error_line(const pc_parser *parser);

#endif

