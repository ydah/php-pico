#include "p2sh.h"

#include "ast.h"
#include "parser.h"

#include <ctype.h>
#include <string.h>
#include <unistd.h>

typedef struct repl_scan {
    unsigned parentheses;
    unsigned brackets;
    unsigned braces;
    char quote;
    int escaped;
    int block_comment;
} repl_scan;

static void scan_line(repl_scan *scan, const char *line, size_t length) {
    size_t i;
    int line_comment = 0;
    for (i = 0U; i < length && !line_comment; i++) {
        char byte = line[i];
        char next = i + 1U < length ? line[i + 1U] : '\0';
        if (scan->block_comment) {
            if (byte == '*' && next == '/') {
                scan->block_comment = 0;
                i++;
            }
            continue;
        }
        if (scan->quote != '\0') {
            if (scan->escaped) {
                scan->escaped = 0;
            } else if (byte == '\\') {
                scan->escaped = 1;
            } else if (byte == scan->quote) {
                scan->quote = '\0';
            }
            continue;
        }
        if ((byte == '/' && next == '/') || byte == '#') {
            line_comment = 1;
        } else if (byte == '/' && next == '*') {
            scan->block_comment = 1;
            i++;
        } else if (byte == '\'' || byte == '"') {
            scan->quote = byte;
        } else if (byte == '(') {
            scan->parentheses++;
        } else if (byte == ')' && scan->parentheses != 0U) {
            scan->parentheses--;
        } else if (byte == '[') {
            scan->brackets++;
        } else if (byte == ']' && scan->brackets != 0U) {
            scan->brackets--;
        } else if (byte == '{') {
            scan->braces++;
        } else if (byte == '}' && scan->braces != 0U) {
            scan->braces--;
        }
    }
}

static int scan_complete(const repl_scan *scan) {
    return scan->parentheses == 0U && scan->brackets == 0U &&
           scan->braces == 0U && scan->quote == '\0' &&
           !scan->block_comment;
}

static int grow_chunk(char **chunk, size_t *capacity, size_t needed) {
    size_t next = *capacity == 0U ? 512U : *capacity * 2U;
    char *resized;
    if (next < needed) next = needed;
    if (next > PPHP_STR_MAX + 1U) next = PPHP_STR_MAX + 1U;
    if (next < needed) return 0;
    resized = pphp_realloc(*chunk, next);
    if (resized == NULL) return 0;
    *chunk = resized;
    *capacity = next;
    return 1;
}

static int one_expression(const char *source, size_t length,
                          size_t *expression_length) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    const pc_ast *statement;
    size_t end = length;
    const char *candidate = source;
    size_t candidate_length = length;
    char *terminated = NULL;
    int matches = 0;
    while (end != 0U && isspace((unsigned char)source[end - 1U])) end--;
    if (end != 0U && source[end - 1U] == ';') {
        end--;
    } else {
        terminated = pphp_alloc(length + 2U);
        if (terminated == NULL) return 0;
        memcpy(terminated, source, length);
        terminated[length] = ';';
        terminated[length + 1U] = '\0';
        candidate = terminated;
        candidate_length++;
    }
    while (end != 0U && isspace((unsigned char)source[end - 1U])) end--;
    pc_arena_init(&arena, 1024U);
    pc_parser_init(&parser, &arena, candidate, candidate_length, 1);
    program = pc_parse_program(&parser);
    if (program != NULL) {
        statement = program->as.list.items;
        matches = statement != NULL && statement->next == NULL &&
                  statement->kind == AST_EXPR_STMT;
    }
    pc_arena_destroy(&arena);
    pphp_free(terminated);
    if (matches) *expression_length = end;
    return matches;
}

static int execute_chunk(pphp_state *state, const char *source, size_t length,
                         FILE *errors, unsigned chunk_number) {
    static const char prefix[] = "echo '=> '; var_dump((";
    static const char suffix[] = "));";
    size_t expression_length;
    char name[32];
    const char *execution_source = source;
    size_t execution_length = length;
    char *wrapped = NULL;
    int result;
    if (one_expression(source, length, &expression_length)) {
        execution_length = sizeof(prefix) - 1U + expression_length +
                           sizeof(suffix) - 1U;
        wrapped = pphp_alloc(execution_length + 1U);
        if (wrapped == NULL) {
            fprintf(errors, "php-pico: out of memory preparing REPL expression\n");
            return PPHP_E_NOMEM;
        }
        memcpy(wrapped, prefix, sizeof(prefix) - 1U);
        memcpy(wrapped + sizeof(prefix) - 1U, source, expression_length);
        memcpy(wrapped + sizeof(prefix) - 1U + expression_length,
               suffix, sizeof(suffix));
        execution_source = wrapped;
    }
    (void)snprintf(name, sizeof(name), "repl-%u", chunk_number);
    result = pphp_exec_source_mode(state, execution_source, execution_length,
                                   name, 1);
    pphp_free(wrapped);
    if (result != PPHP_OK) {
        fprintf(errors, "%s", pphp_last_error(state));
        if (pphp_last_error_line(state) != 0U) {
            fprintf(errors, " on line %u", pphp_last_error_line(state));
        }
        fputc('\n', errors);
    }
    return result;
}

int pphp_host_repl(pphp_state *state, FILE *input, FILE *output,
                   FILE *errors) {
    char line[512];
    char *chunk = NULL;
    size_t length = 0U;
    size_t capacity = 0U;
    unsigned chunk_number = 1U;
    int interactive;
    repl_scan scan;
    if (state == NULL || input == NULL || output == NULL || errors == NULL) {
        return PPHP_E_RUNTIME;
    }
    memset(&scan, 0, sizeof(scan));
    interactive = input == stdin && isatty(STDIN_FILENO);
    if (interactive) {
        fputs("php-pico " PPHP_VERSION " interactive shell\n", output);
    }
    for (;;) {
        size_t line_length;
        if (interactive) {
            fputs(length == 0U ? "pico> " : "....> ", output);
            fflush(output);
        }
        if (fgets(line, sizeof(line), input) == NULL) break;
        line_length = strlen(line);
        if (length == 0U &&
            ((line_length == 5U && memcmp(line, "quit\n", 5U) == 0) ||
             (line_length == 5U && memcmp(line, "exit\n", 5U) == 0))) {
            break;
        }
        if (line_length > PPHP_STR_MAX - length ||
            !grow_chunk(&chunk, &capacity, length + line_length + 1U)) {
            fprintf(errors, "php-pico: REPL chunk exceeds configured limit\n");
            length = 0U;
            memset(&scan, 0, sizeof(scan));
            continue;
        }
        memcpy(chunk + length, line, line_length);
        length += line_length;
        chunk[length] = '\0';
        scan_line(&scan, line, line_length);
        if (!scan_complete(&scan)) continue;
        (void)execute_chunk(state, chunk, length, errors, chunk_number++);
        length = 0U;
        memset(&scan, 0, sizeof(scan));
        if (pphp_exit_requested(state)) break;
    }
    if (length != 0U) {
        (void)execute_chunk(state, chunk, length, errors, chunk_number);
    }
    pphp_free(chunk);
    return pphp_exit_requested(state) ? pphp_exit_status(state) : PPHP_OK;
}
