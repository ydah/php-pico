#include "pphp/pphp.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t host_pool[PPHP_HEAP_SIZE];

static void print_usage(FILE *stream) {
    fprintf(stream,
            "Usage: php-pico [--version]\n"
            "       php-pico --tokens file.php\n"
            "       php-pico --ast file.php\n");
}

static char *read_file(const char *path, size_t *length) {
    FILE *file;
    long size;
    char *contents;
    size_t read_count;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "php-pico: cannot open %s\n", path);
        return NULL;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (size = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "php-pico: cannot determine size of %s\n", path);
        (void)fclose(file);
        return NULL;
    }
    contents = pphp_alloc((size_t)size + 1U);
    if (contents == NULL) {
        fprintf(stderr, "php-pico: file is too large for the configured heap\n");
        (void)fclose(file);
        return NULL;
    }
    read_count = fread(contents, 1U, (size_t)size, file);
    (void)fclose(file);
    if (read_count != (size_t)size) {
        fprintf(stderr, "php-pico: cannot read %s\n", path);
        pphp_free(contents);
        return NULL;
    }
    contents[read_count] = '\0';
    *length = read_count;
    return contents;
}

static void print_lexeme(const pc_token *token) {
    size_t i;
    putchar('"');
    for (i = 0U; i < token->length; i++) {
        unsigned char value = (unsigned char)token->start[i];
        if (value == '\n') {
            fputs("\\n", stdout);
        } else if (value == '\r') {
            fputs("\\r", stdout);
        } else if (value == '\t') {
            fputs("\\t", stdout);
        } else if (value == '\\' || value == '"') {
            putchar('\\');
            putchar((int)value);
        } else if (value < 0x20U || value == 0x7fU) {
            printf("\\x%02x", value);
        } else {
            putchar((int)value);
        }
    }
    putchar('"');
}

static int dump_tokens(const char *source, size_t length) {
    pc_lexer lexer;
    pc_token token;
    pc_lexer_init(&lexer, source, length, 0);
    do {
        token = pc_lexer_next(&lexer);
        printf("%u:%u %-18s ", token.line, token.column, pc_token_name(token.type));
        print_lexeme(&token);
        putchar('\n');
        if (token.type == T_ERROR) {
            fprintf(stderr, "Parse error: %s on line %u\n",
                    pc_lexer_error(&lexer), token.line);
            return PPHP_E_PARSE;
        }
    } while (token.type != T_EOF);
    return PPHP_OK;
}

static int dump_ast(const char *source, size_t length, const char *path) {
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pc_arena_init(&arena, 4096U);
    pc_parser_init(&parser, &arena, source, length, 0);
    program = pc_parse_program(&parser);
    if (program == NULL) {
        fprintf(stderr, "Parse error: %s in %s on line %u\n",
                pc_parser_error(&parser), path, pc_parser_error_line(&parser));
        pc_arena_destroy(&arena);
        return PPHP_E_PARSE;
    }
    pc_ast_dump(stdout, program);
    pc_arena_destroy(&arena);
    return PPHP_OK;
}

int main(int argc, char **argv) {
    pphp_pool_init(host_pool, sizeof(host_pool));
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("php-pico %s\n", PPHP_VERSION);
        return 0;
    }
    if (argc == 3 && (strcmp(argv[1], "--tokens") == 0 ||
                      strcmp(argv[1], "--ast") == 0)) {
        size_t length = 0U;
        char *source = read_file(argv[2], &length);
        int result;
        if (source == NULL) {
            return PPHP_E_IO;
        }
        result = strcmp(argv[1], "--tokens") == 0
                     ? dump_tokens(source, length)
                     : dump_ast(source, length, argv[2]);
        pphp_free(source);
        return result;
    }
    print_usage(argc == 1 ? stdout : stderr);
    return argc == 1 ? 0 : 2;
}
