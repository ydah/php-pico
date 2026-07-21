#include "pphp/pphp.h"
#include "pphp/hal.h"
#include "state.h"

#include "pico/stdlib.h"

#include <stdio.h>
#include <string.h>

static uint8_t heap_pool[PPHP_HEAP_SIZE];
static pphp_state *runtime;

static void console_output(void *context, const char *bytes, size_t length) {
    (void)context;
    hal_console_write(bytes, length);
}

static bool tick_callback(struct repeating_timer *timer) {
    (void)timer;
    pphp_tick(runtime);
    return true;
}

static void report_error(void) {
    char line[32];
    int length;
    hal_console_write(pphp_last_error(runtime),
                      strlen(pphp_last_error(runtime)));
    length = snprintf(line, sizeof(line), " on line %lu\r\n",
                      (unsigned long)pphp_last_error_line(runtime));
    if (length > 0) hal_console_write(line, (size_t)length);
}

int main(void) {
    struct repeating_timer tick;
    char line[512];
    size_t length = 0U;
    (void)hal_init();
    pphp_pool_init(heap_pool, sizeof(heap_pool));
    runtime = pphp_open(NULL, 0U);
    if (runtime == NULL) {
        hal_console_write("php-pico: VM initialization failed\r\n", 36U);
        for (;;) tight_loop_contents();
    }
    pphp_set_output(runtime, console_output, NULL);
    (void)add_repeating_timer_ms(1, tick_callback, NULL, &tick);
    hal_console_write("php-pico " PPHP_VERSION "\r\npico$ ",
                      sizeof("php-pico " PPHP_VERSION "\r\npico$ ") - 1U);
    for (;;) {
        uint8_t byte;
        int count = hal_console_read(&byte, 1U);
        if (count == 0) {
            __wfe();
            continue;
        }
        if (byte == 3U) {
            length = 0U;
            hal_console_write("^C\r\npico$ ", 11U);
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            int status;
            hal_console_write("\r\n", 2U);
            line[length] = '\0';
            status = pphp_exec_source_mode(runtime, line, length,
                                           "<console>", 1);
            if (status != PPHP_OK) report_error();
            length = 0U;
            hal_console_write("pico$ ", 6U);
            continue;
        }
        if ((byte == 8U || byte == 127U) && length != 0U) {
            length--;
            hal_console_write("\b \b", 3U);
        } else if (byte >= 32U && length + 1U < sizeof(line)) {
            line[length++] = (char)byte;
            hal_console_write((const char *)&byte, 1U);
        }
    }
}
