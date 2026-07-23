#include "test.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

int pphp_snprintf(char *buffer, size_t capacity, const char *format, ...);
int vprintf(const char *format, va_list arguments);

static char console_output[128];
static size_t console_length;

int stdio_putchar(int byte) {
    if (console_length < sizeof(console_output) - 1U) {
        console_output[console_length] = (char)byte;
    }
    console_length++;
    return byte;
}

int stdio_put_string(const char *string, int length, bool newline,
                     bool translate_crlf) {
    int index;
    (void)translate_crlf;
    for (index = 0; index < length; index++) {
        (void)stdio_putchar((unsigned char)string[index]);
    }
    if (newline) (void)stdio_putchar('\n');
    return length + (newline ? 1 : 0);
}

static int console_printf(const char *format, ...) {
    va_list arguments;
    int result;
    console_length = 0U;
    va_start(arguments, format);
    result = vprintf(format, arguments);
    va_end(arguments);
    console_output[console_length < sizeof(console_output)
                       ? console_length : sizeof(console_output) - 1U] = '\0';
    return result;
}

TEST(formats_device_integer_and_string_closure) {
    char buffer[160];
    int length = pphp_snprintf(
        buffer, sizeof(buffer),
        "%08d|%-8u|%08x|%X|%o|%c|%.3s|%.*s|%zu|%ld|%%",
        -42, 7U, 255U, 48879U, 8U, 'Z', "abcdef", 2, "xyz",
        (size_t)17U, -123L);
    ASSERT_EQ(strlen("-0000042|7       |000000ff|BEEF|10|Z|abc|xy|17|-123|%"),
              (size_t)length);
    ASSERT_STR("-0000042|7       |000000ff|BEEF|10|Z|abc|xy|17|-123|%",
               buffer);
}

TEST(honors_dynamic_width_precision_and_zero_precision) {
    char buffer[64];
    ASSERT_EQ(12, pphp_snprintf(buffer, sizeof(buffer), "[%*d][%.0u]",
                                -8, 12, 0U));
    ASSERT_STR("[12      ][]", buffer);
}

TEST(truncates_without_changing_logical_length) {
    char buffer[6];
    ASSERT_EQ(11, pphp_snprintf(buffer, sizeof(buffer), "error:%s", "value"));
    ASSERT_STR("error", buffer);
    ASSERT_EQ(11, pphp_snprintf(NULL, 0U, "error:%s", "value"));
}

TEST(console_path_uses_the_same_formatter) {
    ASSERT_EQ(10, console_printf("%04d:%-5s", 7, "ok"));
    ASSERT_STR("0007:ok   ", console_output);
}

int main(void) {
    const test_case tests[] = {
        {"integer and string closure",
         formats_device_integer_and_string_closure},
        {"dynamic width and precision",
         honors_dynamic_width_precision_and_zero_precision},
        {"bounded truncation", truncates_without_changing_logical_length},
        {"console formatting", console_path_uses_the_same_formatter},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
