#define _POSIX_C_SOURCE 200809L

#include "p2sh.h"

#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "files.h"
#include "pbc.h"
#include "pphp/hal.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
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

static char *skip_space(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    return text;
}

static void trim_end(char *text) {
    size_t length = strlen(text);
    while (length != 0U && isspace((unsigned char)text[length - 1U])) {
        text[--length] = '\0';
    }
}

static int copy_file(const char *source, const char *target) {
    FILE *input = fopen(source, "rb");
    FILE *output;
    char buffer[512];
    size_t count;
    int ok = 1;
    if (input == NULL) return 0;
    output = fopen(target, "wb");
    if (output == NULL) {
        (void)fclose(input);
        return 0;
    }
    while ((count = fread(buffer, 1U, sizeof(buffer), input)) != 0U) {
        if (fwrite(buffer, 1U, count, output) != count) {
            ok = 0;
            break;
        }
    }
    if (ferror(input)) ok = 0;
    if (fclose(input) != 0) ok = 0;
    if (fclose(output) != 0) ok = 0;
    return ok;
}

static int remove_tree(const char *path) {
    struct stat info;
    if (lstat(path, &info) != 0) return 0;
    if (S_ISDIR(info.st_mode)) {
        DIR *directory = opendir(path);
        struct dirent *entry;
        int ok = directory != NULL;
        while (ok && (entry = readdir(directory)) != NULL) {
            char child[1024];
            int length;
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) continue;
            length = snprintf(child, sizeof(child), "%s/%s", path,
                              entry->d_name);
            if (length < 0 || (size_t)length >= sizeof(child) ||
                !remove_tree(child)) ok = 0;
        }
        if (directory != NULL && closedir(directory) != 0) ok = 0;
        return ok && rmdir(path) == 0;
    }
    return unlink(path) == 0;
}

static void shell_error(FILE *errors, const char *operation,
                        const char *path) {
    fprintf(errors, "pico: %s %s: %s\n", operation,
            path == NULL ? "" : path, strerror(errno));
}

static int shell_ls(const char *path, FILE *output, FILE *errors) {
    DIR *directory = opendir(path == NULL || *path == '\0' ? "." : path);
    struct dirent *entry;
    if (directory == NULL) {
        shell_error(errors, "ls", path);
        return PPHP_E_IO;
    }
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;
        fprintf(output, "%s\n", entry->d_name);
    }
    (void)closedir(directory);
    return PPHP_OK;
}

static int shell_cat(const char *path, FILE *output, FILE *errors) {
    FILE *file = fopen(path, "rb");
    char buffer[512];
    size_t count;
    if (file == NULL) {
        shell_error(errors, "cat", path);
        return PPHP_E_IO;
    }
    while ((count = fread(buffer, 1U, sizeof(buffer), file)) != 0U) {
        (void)fwrite(buffer, 1U, count, output);
    }
    (void)fclose(file);
    return PPHP_OK;
}

static int shell_php(pphp_state *state, const char *path, FILE *errors) {
    char *bytes;
    size_t length;
    int result;
    if (!pphp_file_read_all(path, &bytes, &length)) {
        shell_error(errors, "php", path);
        return PPHP_E_IO;
    }
    if (length >= 4U && memcmp(bytes, "PPBC", 4U) == 0) {
        result = pphp_exec_pbc_owned(state, bytes, length);
        bytes = NULL;
    } else {
        result = pphp_exec_source_mode(state, bytes, length, path, 1);
    }
    pphp_free(bytes);
    if (result != PPHP_OK) {
        fprintf(errors, "%s on line %u\n", pphp_last_error(state),
                pphp_last_error_line(state));
    }
    return result;
}

static int shell_phpc(const char *input_path, const char *output_path,
                      FILE *errors) {
    char *source;
    size_t length;
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pc_codegen_error codegen_error;
    pmodule module;
    int result;
    if (!pphp_file_read_all(input_path, &source, &length)) {
        shell_error(errors, "phpc", input_path);
        return PPHP_E_IO;
    }
    pc_arena_init(&arena, 4096U);
    pc_parser_init(&parser, &arena, source, length, 0);
    program = pc_parse_program(&parser);
    if (program == NULL) {
        fprintf(errors, "Parse error: %s in %s on line %u\n",
                pc_parser_error(&parser), input_path,
                pc_parser_error_line(&parser));
        result = PPHP_E_PARSE;
    } else if (!pc_codegen_program(program, &module, &codegen_error)) {
        fprintf(errors, "Compile error: %s in %s on line %u\n",
                codegen_error.message, input_path, codegen_error.line);
        result = PPHP_E_PARSE;
    } else {
        result = pphp_pbc_write_file(&module, output_path);
        pmodule_destroy(&module);
        if (result != PPHP_OK) shell_error(errors, "phpc", output_path);
    }
    pc_arena_destroy(&arena);
    pphp_free(source);
    return result;
}

static int execute_shell_php(pphp_state *state, const char *line,
                             FILE *errors, unsigned number) {
    char chunk_name[32];
    int result;
    (void)snprintf(chunk_name, sizeof(chunk_name), "shell-%u", number);
    result = pphp_exec_source_mode(state, line, strlen(line), chunk_name, 1);
    if (result != PPHP_OK) {
        fprintf(errors, "%s on line %u\n", pphp_last_error(state),
                pphp_last_error_line(state));
    }
    return result;
}

int pphp_host_shell(pphp_state *state, FILE *input, FILE *output,
                    FILE *errors) {
    char line[1024];
    unsigned php_chunk = 1U;
    int interactive;
    if (state == NULL || input == NULL || output == NULL || errors == NULL) {
        return PPHP_E_RUNTIME;
    }
    interactive = input == stdin && isatty(STDIN_FILENO);
    if (interactive) fprintf(output, "php-pico %s P2Sh\n", PPHP_VERSION);
    for (;;) {
        char *command;
        char *argument;
        if (interactive) {
            fputs("pico$ ", output);
            fflush(output);
        }
        if (fgets(line, sizeof(line), input) == NULL) break;
        trim_end(line);
        command = skip_space(line);
        if (*command == '\0') continue;
        argument = command;
        while (*argument != '\0' && !isspace((unsigned char)*argument)) {
            argument++;
        }
        if (*argument != '\0') *argument++ = '\0';
        argument = skip_space(argument);
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            break;
        } else if (strcmp(command, "version") == 0) {
            fprintf(output, "php-pico %s\n", PPHP_VERSION);
        } else if (strcmp(command, "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) == NULL) shell_error(errors, "pwd", NULL);
            else fprintf(output, "%s\n", cwd);
        } else if (strcmp(command, "cd") == 0) {
            if (*argument == '\0' || chdir(argument) != 0) {
                shell_error(errors, "cd", argument);
            }
        } else if (strcmp(command, "ls") == 0) {
            (void)shell_ls(argument, output, errors);
        } else if (strcmp(command, "cat") == 0) {
            if (*argument == '\0') fputs("usage: cat FILE\n", errors);
            else (void)shell_cat(argument, output, errors);
        } else if (strcmp(command, "mkdir") == 0) {
            if (*argument == '\0' || mkdir(argument, 0777) != 0) {
                shell_error(errors, "mkdir", argument);
            }
        } else if (strcmp(command, "rm") == 0) {
            int recursive = strncmp(argument, "-r", 2U) == 0 &&
                            isspace((unsigned char)argument[2]);
            const char *path = recursive ? skip_space(argument + 2U) : argument;
            if (*path == '\0' ||
                (recursive ? !remove_tree(path) : unlink(path) != 0)) {
                shell_error(errors, "rm", path);
            }
        } else if (strcmp(command, "mv") == 0 || strcmp(command, "cp") == 0) {
            char *target = argument;
            while (*target != '\0' && !isspace((unsigned char)*target)) target++;
            if (*target != '\0') *target++ = '\0';
            target = skip_space(target);
            if (*argument == '\0' || *target == '\0' ||
                (strcmp(command, "mv") == 0
                     ? rename(argument, target) != 0
                     : !copy_file(argument, target))) {
                shell_error(errors, command, argument);
            }
        } else if (strcmp(command, "echo") == 0) {
            char *redirect = strstr(argument, ">");
            if (redirect == NULL) {
                fprintf(output, "%s\n", argument);
            } else {
                FILE *file;
                char *path;
                *redirect = '\0';
                trim_end(argument);
                path = skip_space(redirect + 1U);
                file = fopen(path, "wb");
                if (file == NULL) shell_error(errors, "echo", path);
                else {
                    (void)fwrite(argument, 1U, strlen(argument), file);
                    (void)fputc('\n', file);
                    (void)fclose(file);
                }
            }
        } else if (strcmp(command, "free") == 0) {
            pphp_pool_stats stats = pphp_pool_get_stats();
            fprintf(output,
                    "total=%zu used=%zu free=%zu largest=%zu fragments=%zu\n",
                    stats.total, stats.used, stats.free, stats.largest_free,
                    stats.fragments);
#if PPHP_RC_DEBUG
        } else if (strcmp(command, "rccheck") == 0) {
            pphp_rc_check_result check;
            if (pphp_rc_check(state, &check)) {
                fprintf(output, "rccheck: OK checked=%zu\n", check.checked);
            } else if (check.status == PPHP_RC_CHECK_MISMATCH) {
                fprintf(output,
                        "rccheck: mismatch target=%p actual=%u expected=%zu "
                        "checked=%zu\n",
                        (const void *)check.target, (unsigned)check.actual,
                        check.expected, check.checked);
            } else {
                fprintf(output, "rccheck: error status=%d checked=%zu\n",
                        check.status, check.checked);
            }
#endif
        } else if (strcmp(command, "reboot") == 0) {
            hal_reset();
        } else if (strcmp(command, "php") == 0) {
            if (*argument == '\0') fputs("usage: php FILE\n", errors);
            else (void)shell_php(state, argument, errors);
        } else if (strcmp(command, "phpc") == 0) {
            char *target = argument;
            while (*target != '\0' && !isspace((unsigned char)*target)) target++;
            if (*target != '\0') *target++ = '\0';
            target = skip_space(target);
            if (*argument == '\0' || *target == '\0') {
                fputs("usage: phpc INPUT OUTPUT\n", errors);
            } else {
                (void)shell_phpc(argument, target, errors);
            }
        } else if (strcmp(command, "repl") == 0) {
            (void)pphp_host_repl(state, input, output, errors);
            break;
        } else {
            if (*argument != '\0') argument[-1] = ' ';
            (void)execute_shell_php(state, command, errors, php_chunk++);
        }
        if (pphp_exit_requested(state)) break;
    }
    return pphp_exit_requested(state) ? pphp_exit_status(state) : PPHP_OK;
}
