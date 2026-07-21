#include "pphp/fs.h"
#include "pphp/hal.h"
#include "pphp/pphp.h"
#include "p2sh_device.h"
#include "state.h"

#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <stdio.h>
#include <string.h>

#define CONSOLE_LINE_SIZE 512U
#define HISTORY_COUNT 8U

static uint8_t heap_pool[PPHP_HEAP_SIZE];
static pphp_state *runtime;

static void console_output(void *context, const char *bytes, size_t length) {
    (void)context;
    hal_console_write(bytes, length);
}

static void write_text(const char *text) {
    hal_console_write(text, strlen(text));
}

static bool tick_callback(struct repeating_timer *timer) {
    (void)timer;
    pphp_tick(runtime);
    return true;
}

static void report_error(void) {
    char line[352];
    int length = snprintf(line, sizeof(line), "%s on line %lu\r\n",
                          pphp_last_error(runtime),
                          (unsigned long)pphp_last_error_line(runtime));
    if (length > 0) hal_console_write(line, (size_t)length);
}

static int wait_for_recovery(void) {
    uint64_t deadline;
    int skip = hal_bootsel_pressed();
    if (skip) {
        write_text("BOOTSEL held: auto-start skipped\r\n");
        return 1;
    }
    write_text("Press Ctrl-C within 3 seconds to skip auto-start...\r\n");
    deadline = hal_time_us() + UINT64_C(3000000);
    while (hal_time_us() < deadline) {
        uint8_t byte;
        if (hal_console_read(&byte, 1U) > 0 && byte == 3U) {
            write_text("^C auto-start skipped\r\n");
            skip = 1;
            break;
        }
        hal_sleep_ms(10U);
    }
    return skip;
}

static void auto_start(void) {
    int result = PPHP_OK;
    if (pphp_fs_exists("/home/boot.php")) {
        result = pphp_p2sh_run_file(runtime, "/home/boot.php");
    }
    if (result != PPHP_OK) return;
    if (pphp_fs_exists("/home/app.pbc")) {
        (void)pphp_p2sh_run_file(runtime, "/home/app.pbc");
    } else if (pphp_fs_exists("/home/app.php")) {
        (void)pphp_p2sh_run_file(runtime, "/home/app.php");
    }
}

static const char *prompt_for(int repl_mode) {
    return repl_mode ? "pico> " : "pico$ ";
}

static void redraw_line(const char *prompt, const char *line, size_t length,
                        size_t cursor) {
    size_t move;
    write_text("\r\033[2K");
    write_text(prompt);
    hal_console_write(line, length);
    for (move = cursor; move < length; move++) write_text("\b");
}

static void history_store(char history[HISTORY_COUNT][CONSOLE_LINE_SIZE],
                          size_t *count, const char *line) {
    if (*line == '\0') return;
    if (*count != 0U && strcmp(history[*count - 1U], line) == 0) return;
    if (*count == HISTORY_COUNT) {
        memmove(history[0], history[1],
                (HISTORY_COUNT - 1U) * CONSOLE_LINE_SIZE);
        (*count)--;
    }
    memcpy(history[*count], line, strlen(line) + 1U);
    (*count)++;
}

static void history_load(char *line, size_t *length, size_t *cursor,
                         char history[HISTORY_COUNT][CONSOLE_LINE_SIZE],
                         size_t history_count, size_t history_index,
                         const char *prompt) {
    if (history_index >= history_count) {
        line[0] = '\0';
        *length = 0U;
    } else {
        *length = strlen(history[history_index]);
        memcpy(line, history[history_index], *length + 1U);
    }
    *cursor = *length;
    redraw_line(prompt, line, *length, *cursor);
}

static void execute_repl_line(char *line) {
    static unsigned chunk = 1U;
    char name[32];
    if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) return;
    (void)snprintf(name, sizeof(name), "repl-%u", chunk++);
    if (pphp_exec_source_mode(runtime, line, strlen(line), name, 1) != PPHP_OK) {
        report_error();
    }
}

int main(void) {
    struct repeating_timer tick;
    char line[CONSOLE_LINE_SIZE];
    char history[HISTORY_COUNT][CONSOLE_LINE_SIZE];
    size_t length = 0U;
    size_t cursor = 0U;
    size_t history_count = 0U;
    size_t history_index = 0U;
    unsigned escape_state = 0U;
    int repl_mode = 0;
    (void)hal_init();
    pphp_pool_init(heap_pool, sizeof(heap_pool));
    runtime = pphp_open(NULL, 0U);
    if (runtime == NULL) {
        write_text("php-pico: VM initialization failed\r\n");
        for (;;) tight_loop_contents();
    }
    pphp_set_output(runtime, console_output, NULL);
    (void)add_repeating_timer_ms(1, tick_callback, NULL, &tick);
    if (!pphp_fs_mount()) write_text("php-pico: filesystem mount failed\r\n");
    pphp_p2sh_init();
    write_text("php-pico " PPHP_VERSION " P2Sh\r\n");
    if (!wait_for_recovery()) auto_start();
    write_text(prompt_for(repl_mode));
    line[0] = '\0';
    for (;;) {
        uint8_t byte;
        int count = hal_console_read(&byte, 1U);
        const char *prompt = prompt_for(repl_mode);
        if (count == 0) {
            __wfe();
            continue;
        }
        if (escape_state == 1U) {
            escape_state = byte == '[' ? 2U : 0U;
            continue;
        }
        if (escape_state == 2U) {
            escape_state = 0U;
            if (byte == 'A' && history_index > 0U) {
                history_index--;
                history_load(line, &length, &cursor, history, history_count,
                             history_index, prompt);
            } else if (byte == 'B' && history_index < history_count) {
                history_index++;
                history_load(line, &length, &cursor, history, history_count,
                             history_index, prompt);
            } else if (byte == 'C' && cursor < length) {
                hal_console_write(line + cursor, 1U);
                cursor++;
            } else if (byte == 'D' && cursor > 0U) {
                write_text("\b");
                cursor--;
            }
            continue;
        }
        if (byte == 27U) {
            escape_state = 1U;
            continue;
        }
        if (byte == 3U) {
            length = 0U;
            cursor = 0U;
            line[0] = '\0';
            history_index = history_count;
            write_text("^C\r\n");
            write_text(prompt);
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            write_text("\r\n");
            line[length] = '\0';
            history_store(history, &history_count, line);
            history_index = history_count;
            if (repl_mode &&
                (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)) {
                repl_mode = 0;
            } else if (repl_mode) {
                execute_repl_line(line);
            } else if (pphp_p2sh_execute(runtime, line) ==
                       PPHP_P2SH_ENTER_REPL) {
                repl_mode = 1;
            }
            length = 0U;
            cursor = 0U;
            line[0] = '\0';
            write_text(prompt_for(repl_mode));
            continue;
        }
        if ((byte == 8U || byte == 127U) && cursor != 0U) {
            size_t move;
            memmove(line + cursor - 1U, line + cursor, length - cursor);
            cursor--;
            length--;
            line[length] = '\0';
            write_text("\b");
            hal_console_write(line + cursor, length - cursor);
            write_text(" ");
            for (move = cursor; move <= length; move++) write_text("\b");
        } else if (byte >= 32U && length + 1U < sizeof(line)) {
            size_t move;
            memmove(line + cursor + 1U, line + cursor, length - cursor);
            line[cursor] = (char)byte;
            cursor++;
            length++;
            line[length] = '\0';
            hal_console_write(line + cursor - 1U, length - cursor + 1U);
            for (move = cursor; move < length; move++) write_text("\b");
        }
    }
}
