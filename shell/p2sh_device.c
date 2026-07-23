#include "p2sh_device.h"

#include "files.h"
#if PPHP_ENABLE_COMPILER
#include "ast.h"
#include "codegen.h"
#include "parser.h"
#endif
#include "pbc.h"
#include "pphp/fs.h"
#include "pphp/hal.h"
#include "state.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define P2SH_PATH_MAX 256U

static char current_directory[P2SH_PATH_MAX];
static unsigned php_chunk = 1U;

static int ascii_space(unsigned char byte) {
    return byte == ' ' || (byte >= '\t' && byte <= '\r');
}

static int ascii_alnum(unsigned char byte) {
    return (byte >= '0' && byte <= '9') ||
           (byte >= 'A' && byte <= 'Z') ||
           (byte >= 'a' && byte <= 'z');
}

static void write_bytes(const char *bytes, size_t length) {
    hal_console_write(bytes, length);
}

static void write_text(const char *text) {
    write_bytes(text, strlen(text));
}

static void write_line(const char *text) {
    write_text(text);
    write_text("\r\n");
}

static char *skip_space(char *text) {
    while (*text != '\0' && ascii_space((unsigned char)*text)) text++;
    return text;
}

static void trim_end(char *text) {
    size_t length = strlen(text);
    while (length != 0U && ascii_space((unsigned char)text[length - 1U])) {
        text[--length] = '\0';
    }
}

static int append_component(char *path, size_t *length,
                            const char *component, size_t component_length) {
    if (component_length == 0U ||
        (component_length == 1U && component[0] == '.')) return 1;
    if (component_length == 2U && component[0] == '.' && component[1] == '.') {
        if (*length > 1U) {
            while (*length > 1U && path[*length - 1U] != '/') (*length)--;
            if (*length > 1U) (*length)--;
        }
        path[*length] = '\0';
        return 1;
    }
    if (*length > 1U && path[*length - 1U] != '/') {
        if (*length + 1U >= P2SH_PATH_MAX) return 0;
        path[(*length)++] = '/';
    }
    if (*length + component_length >= P2SH_PATH_MAX) return 0;
    memcpy(path + *length, component, component_length);
    *length += component_length;
    path[*length] = '\0';
    return 1;
}

static int normalize_path(const char *input, char *output) {
    const char *cursor;
    size_t length;
    if (input == NULL || *input == '\0') input = current_directory;
    if (*input == '/') {
        output[0] = '/';
        output[1] = '\0';
        length = 1U;
        cursor = input + 1U;
    } else {
        length = strlen(current_directory);
        if (length >= P2SH_PATH_MAX) return 0;
        memcpy(output, current_directory, length + 1U);
        cursor = input;
    }
    while (*cursor != '\0') {
        const char *start;
        size_t component_length;
        while (*cursor == '/') cursor++;
        start = cursor;
        while (*cursor != '\0' && *cursor != '/') cursor++;
        component_length = (size_t)(cursor - start);
        if (!append_component(output, &length, start, component_length)) {
            return 0;
        }
    }
    return 1;
}

static int resolve_argument(const char *argument, char *path) {
    if (argument == NULL || *argument == '\0' ||
        !normalize_path(argument, path)) {
        write_line("pico: invalid or overlong path");
        return 0;
    }
    return 1;
}

static int copy_file(const char *source, const char *target) {
    pphp_file *input = pphp_fs_open(source, "rb");
    pphp_file *output;
    char buffer[256];
    int ok = 1;
    if (input == NULL) return 0;
    output = pphp_fs_open(target, "wb");
    if (output == NULL) {
        (void)pphp_fs_close(input);
        return 0;
    }
    for (;;) {
        int64_t count = pphp_fs_read(input, buffer, sizeof(buffer));
        if (count < 0) {
            ok = 0;
            break;
        }
        if (count == 0) break;
        if (pphp_fs_write(output, buffer, (size_t)count) != count) {
            ok = 0;
            break;
        }
    }
    if (!pphp_fs_close(input)) ok = 0;
    if (!pphp_fs_close(output)) ok = 0;
    return ok;
}

static int remove_tree(const char *path, unsigned depth) {
    pphp_dir *directory;
    char entry[256];
    int read_result;
    if (!pphp_fs_is_dir(path)) return pphp_fs_remove(path);
    if (depth >= 16U) return 0;
    directory = pphp_fs_dir_open(path);
    if (directory == NULL) return 0;
    while ((read_result = pphp_fs_dir_read(directory, entry, sizeof(entry),
                                           NULL)) > 0) {
        char child[P2SH_PATH_MAX];
        int length;
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        length = snprintf(child, sizeof(child), "%s/%s", path, entry);
        if (length < 0 || (size_t)length >= sizeof(child) ||
            !remove_tree(child, depth + 1U)) {
            (void)pphp_fs_dir_close(directory);
            return 0;
        }
    }
    if (read_result < 0 || !pphp_fs_dir_close(directory)) return 0;
    return pphp_fs_rmdir(path);
}

int pphp_p2sh_run_file(pphp_state *state, const char *path) {
    char *bytes;
    size_t length;
    int result;
    if (!pphp_file_read_all(path, &bytes, &length)) {
        write_line("pico: php: unable to read file");
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
        char message[352];
        int count = snprintf(message, sizeof(message), "%s on line %lu\r\n",
                             pphp_last_error(state),
                             (unsigned long)pphp_last_error_line(state));
        if (count > 0) write_bytes(message, (size_t)count);
    }
    return result;
}

static int php_process_status(const pphp_state *state, int result) {
    if (result == PPHP_OK && pphp_exit_requested(state)) {
        return (int)((unsigned)pphp_exit_status(state) & 0xffU);
    }
    if (result == PPHP_OK) return 0;
    return result == PPHP_E_PARSE ? 1 : 255;
}

static int phpt_nonce_valid(const char *nonce) {
    size_t length = 0U;
    while (nonce[length] != '\0') {
        if (!ascii_alnum((unsigned char)nonce[length]) || length >= 32U) return 0;
        length++;
    }
    return length != 0U;
}

static int compile_file(const char *input_path, const char *output_path) {
#if PPHP_ENABLE_COMPILER
    char *source;
    size_t length;
    pc_arena arena;
    pc_parser parser;
    pc_ast *program;
    pc_codegen_error codegen_error;
    pmodule module;
    int result;
    if (!pphp_file_read_all(input_path, &source, &length)) {
        write_line("pico: phpc: unable to read input");
        return PPHP_E_IO;
    }
    pc_arena_init(&arena, 4096U);
    pc_parser_init(&parser, &arena, source, length, 0);
    program = pc_parse_program(&parser);
    if (program == NULL) {
        char message[352];
        int count = snprintf(message, sizeof(message),
                             "Parse error: %s on line %lu\r\n",
                             pc_parser_error(&parser),
                             (unsigned long)pc_parser_error_line(&parser));
        if (count > 0) write_bytes(message, (size_t)count);
        result = PPHP_E_PARSE;
    } else if (!pc_codegen_program(program, &module, &codegen_error)) {
        char message[352];
        int count = snprintf(message, sizeof(message),
                             "Compile error: %s on line %lu\r\n",
                             codegen_error.message,
                             (unsigned long)codegen_error.line);
        if (count > 0) write_bytes(message, (size_t)count);
        result = PPHP_E_PARSE;
    } else {
        result = pphp_pbc_write_file(&module, output_path);
        pmodule_destroy(&module);
        if (result != PPHP_OK) write_line("pico: phpc: write failed");
    }
    pc_arena_destroy(&arena);
    pphp_free(source);
    return result;
#else
    (void)input_path;
    (void)output_path;
    write_line("pico: phpc: source compiler is disabled");
    return PPHP_E_PARSE;
#endif
}

static void list_directory(const char *path) {
    pphp_dir *directory = pphp_fs_dir_open(path);
    char entry[256];
    int is_directory;
    int result;
    if (directory == NULL) {
        write_line("pico: ls: unable to open directory");
        return;
    }
    while ((result = pphp_fs_dir_read(directory, entry, sizeof(entry),
                                      &is_directory)) > 0) {
        if (strcmp(entry, ".") == 0 || strcmp(entry, "..") == 0) continue;
        write_text(entry);
        write_line(is_directory ? "/" : "");
    }
    if (result < 0) write_line("pico: ls: read failed");
    (void)pphp_fs_dir_close(directory);
}

static void cat_file(const char *path) {
    pphp_file *file = pphp_fs_open(path, "rb");
    char buffer[256];
    int64_t count;
    if (file == NULL) {
        write_line("pico: cat: unable to open file");
        return;
    }
    while ((count = pphp_fs_read(file, buffer, sizeof(buffer))) > 0) {
        write_bytes(buffer, (size_t)count);
    }
    if (count < 0) write_line("pico: cat: read failed");
    (void)pphp_fs_close(file);
}

static void echo_command(char *argument) {
    char *redirect = argument;
    while (*redirect != '\0' && *redirect != '>') redirect++;
    if (*redirect == '\0') redirect = NULL;
    if (redirect == NULL) {
        write_line(argument);
        return;
    }
    {
        char path[P2SH_PATH_MAX];
        char *target;
        const char *text;
        size_t length;
        pphp_file *file;
        int ok;
        *redirect = '\0';
        trim_end(argument);
        target = skip_space(redirect + 1U);
        if (!resolve_argument(target, path)) return;
        text = argument;
        length = strlen(text);
        if (length >= 2U && ((text[0] == '\'' && text[length - 1U] == '\'') ||
                            (text[0] == '"' && text[length - 1U] == '"'))) {
            text++;
            length -= 2U;
        }
        file = pphp_fs_open(path, "wb");
        if (file == NULL) {
            write_line("pico: echo: write failed");
            return;
        }
        ok = pphp_fs_write(file, text, length) == (int64_t)length &&
             pphp_fs_write(file, "\n", 1U) == 1;
        if (!pphp_fs_close(file)) ok = 0;
        if (!ok) write_line("pico: echo: write failed");
    }
}

static void upload_command(char *argument) {
    char path[P2SH_PATH_MAX];
    char *size_text = argument;
    unsigned long requested;
    unsigned long received = 0UL;
    uint64_t deadline;
    pphp_file *file;
    while (*size_text != '\0' && !ascii_space((unsigned char)*size_text)) {
        size_text++;
    }
    if (*size_text != '\0') *size_text++ = '\0';
    size_text = skip_space(size_text);
    requested = 0UL;
    {
        const char *cursor = size_text;
        while (*cursor >= '0' && *cursor <= '9') {
            unsigned long digit = (unsigned long)(*cursor - '0');
            if (requested > (ULONG_MAX - digit) / 10UL) {
                requested = ULONG_MAX;
                break;
            }
            requested = requested * 10UL + digit;
            cursor++;
        }
        if (*cursor != '\0') requested = ULONG_MAX;
    }
    if (*argument == '\0' || *size_text == '\0' ||
        requested > PPHP_FLASH_FS_SIZE || !resolve_argument(argument, path)) {
        write_line("usage: upload FILE SIZE");
        return;
    }
    file = pphp_fs_open(path, "wb");
    if (file == NULL) {
        write_line("pico: upload: unable to open file");
        return;
    }
    write_line("READY");
    deadline = hal_time_us() + UINT64_C(30000000);
    while (received < requested) {
        uint8_t buffer[256];
        size_t remaining = (size_t)(requested - received);
        size_t capacity = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        int count = hal_console_read(buffer, capacity);
        if (count < 0 ||
            (count > 0 && pphp_fs_write(file, buffer, (size_t)count) != count)) {
            (void)pphp_fs_close(file);
            write_line("pico: upload failed");
            return;
        }
        if (count == 0) {
            if (hal_time_us() >= deadline) {
                (void)pphp_fs_close(file);
                write_line("pico: upload timed out");
                return;
            }
            hal_sleep_ms(1U);
        } else {
            received += (unsigned long)count;
            deadline = hal_time_us() + UINT64_C(30000000);
        }
    }
    write_line(pphp_fs_close(file) ? "OK" : "pico: upload close failed");
}

void pphp_p2sh_init(void) {
    memcpy(current_directory, "/home", sizeof("/home"));
    php_chunk = 1U;
}

int pphp_p2sh_execute(pphp_state *state, char *line) {
    char *command;
    char *argument;
    char path[P2SH_PATH_MAX];
    if (state == NULL || line == NULL) return PPHP_P2SH_DONE;
    trim_end(line);
    command = skip_space(line);
    if (*command == '\0') return PPHP_P2SH_DONE;
    if (*command == '$' || strncmp(command, "<?", 2U) == 0) {
        char name[32];
        (void)snprintf(name, sizeof(name), "shell-%u", php_chunk++);
        if (pphp_exec_source_mode(state, command, strlen(command), name, 1) !=
            PPHP_OK) {
            write_text(pphp_last_error(state));
            write_text("\r\n");
        }
        return PPHP_P2SH_DONE;
    }
    argument = command;
    while (*argument != '\0' && !ascii_space((unsigned char)*argument)) argument++;
    if (*argument != '\0') *argument++ = '\0';
    argument = skip_space(argument);
    if (strcmp(command, "version") == 0) {
        write_line("php-pico " PPHP_VERSION);
    } else if (strcmp(command, "pwd") == 0) {
        write_line(current_directory);
    } else if (strcmp(command, "cd") == 0) {
        if (!resolve_argument(*argument == '\0' ? "/home" : argument, path)) {
            return PPHP_P2SH_DONE;
        }
        if (!pphp_fs_is_dir(path)) write_line("pico: cd: not a directory");
        else memcpy(current_directory, path, strlen(path) + 1U);
    } else if (strcmp(command, "ls") == 0) {
        if (resolve_argument(*argument == '\0' ? current_directory : argument,
                             path)) list_directory(path);
    } else if (strcmp(command, "cat") == 0) {
        if (resolve_argument(argument, path)) cat_file(path);
    } else if (strcmp(command, "mkdir") == 0) {
        if (resolve_argument(argument, path) && !pphp_fs_mkdir(path, 0777U)) {
            write_line("pico: mkdir failed");
        }
    } else if (strcmp(command, "rm") == 0) {
        int recursive = strncmp(argument, "-r", 2U) == 0 &&
                        ascii_space((unsigned char)argument[2]);
        char *target = recursive ? skip_space(argument + 2U) : argument;
        if (resolve_argument(target, path) &&
            !(recursive ? remove_tree(path, 0U) : pphp_fs_remove(path))) {
            write_line("pico: rm failed");
        }
    } else if (strcmp(command, "mv") == 0 || strcmp(command, "cp") == 0) {
        char source[P2SH_PATH_MAX];
        char target[P2SH_PATH_MAX];
        char *second = argument;
        while (*second != '\0' && !ascii_space((unsigned char)*second)) second++;
        if (*second != '\0') *second++ = '\0';
        second = skip_space(second);
        if (!resolve_argument(argument, source) ||
            !resolve_argument(second, target)) return PPHP_P2SH_DONE;
        if (!(strcmp(command, "mv") == 0 ? pphp_fs_rename(source, target)
                                          : copy_file(source, target))) {
            write_line("pico: file operation failed");
        }
    } else if (strcmp(command, "echo") == 0) {
        echo_command(argument);
    } else if (strcmp(command, "upload") == 0) {
        upload_command(argument);
    } else if (strcmp(command, "free") == 0) {
        pphp_pool_stats stats = pphp_pool_get_stats();
        char message[160];
        int count = snprintf(message, sizeof(message),
                             "total=%lu used=%lu free=%lu largest=%lu "
                             "fragments=%lu\r\n",
                             (unsigned long)stats.total,
                             (unsigned long)stats.used,
                             (unsigned long)stats.free,
                             (unsigned long)stats.largest_free,
                             (unsigned long)stats.fragments);
        if (count > 0) write_bytes(message, (size_t)count);
#if PPHP_RC_DEBUG
    } else if (strcmp(command, "rccheck") == 0) {
        pphp_rc_check_result check;
        char message[160];
        int count;
        if (pphp_rc_check(state, &check)) {
            count = snprintf(message, sizeof(message),
                             "rccheck: OK checked=%lu\r\n",
                             (unsigned long)check.checked);
        } else if (check.status == PPHP_RC_CHECK_MISMATCH) {
            count = snprintf(message, sizeof(message),
                             "rccheck: mismatch target=%08lx actual=%u "
                             "expected=%lu checked=%lu\r\n",
                             (unsigned long)(uintptr_t)check.target,
                             (unsigned)check.actual,
                             (unsigned long)check.expected,
                             (unsigned long)check.checked);
        } else {
            count = snprintf(message, sizeof(message),
                             "rccheck: error status=%d checked=%lu\r\n",
                             check.status, (unsigned long)check.checked);
        }
        if (count > 0) write_bytes(message, (size_t)count);
#endif
    } else if (strcmp(command, "reboot") == 0) {
        hal_reset();
    } else if (strcmp(command, "php") == 0) {
        if (resolve_argument(argument, path)) {
            (void)pphp_p2sh_run_file(state, path);
        }
    } else if (strcmp(command, "phpt") == 0) {
        char *nonce = argument;
        char message[80];
        int result = PPHP_E_IO;
        int count;
        while (*nonce != '\0' && !ascii_space((unsigned char)*nonce)) nonce++;
        if (*nonce != '\0') *nonce++ = '\0';
        nonce = skip_space(nonce);
        if (!phpt_nonce_valid(nonce)) {
            write_line("usage: phpt FILE NONCE");
            return PPHP_P2SH_DONE;
        }
        if (resolve_argument(argument, path)) {
            result = pphp_p2sh_run_file(state, path);
        }
        count = snprintf(message, sizeof(message),
                         "\r\n@@PPHP_PHPT_EXIT:%s:%d@@\r\n", nonce,
                         php_process_status(state, result));
        if (count > 0 && (size_t)count < sizeof(message)) {
            write_bytes(message, (size_t)count);
        }
    } else if (strcmp(command, "phpc") == 0) {
        char input[P2SH_PATH_MAX];
        char output[P2SH_PATH_MAX];
        char *second = argument;
        while (*second != '\0' && !ascii_space((unsigned char)*second)) second++;
        if (*second != '\0') *second++ = '\0';
        second = skip_space(second);
        if (resolve_argument(argument, input) &&
            resolve_argument(second, output)) {
            (void)compile_file(input, output);
        }
    } else if (strcmp(command, "repl") == 0) {
#if PPHP_ENABLE_COMPILER
        return PPHP_P2SH_ENTER_REPL;
#else
        write_line("pico: repl: source compiler is disabled");
#endif
    } else {
        write_line("pico: unknown command");
    }
    return PPHP_P2SH_DONE;
}
