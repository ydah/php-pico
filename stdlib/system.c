#include "system.h"

#include "parray.h"
#include "pphp/hal.h"
#include "value_ops.h"
#include "gc.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct utc_fields {
    int year;
    unsigned month;
    unsigned day;
    unsigned hour;
    unsigned minute;
    unsigned second;
    unsigned weekday;
    unsigned iso_weekday;
} utc_fields;

static int name_is(const pstring *name, const char *expected) {
    return ps_equal_bytes(name, expected, strlen(expected));
}

static int invalid_arguments(pphp_state *state, const pstring *name) {
    pphp_runtime_error(state, 0U, "%.*s() received invalid arguments",
                       (int)name->length, ps_data(name));
    return -1;
}

static uint32_t next_random(pphp_state *state) {
    uint32_t value = state->random_state;
    if (value == 0U) value = UINT32_C(0x6d2b79f5);
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    state->random_state = value;
    return value;
}

static uint32_t bounded_random(pphp_state *state, uint32_t bound) {
    uint32_t threshold;
    uint32_t value;
    if (bound == 0U) return next_random(state);
    threshold = (uint32_t)(-bound) % bound;
    do {
        value = next_random(state);
    } while (value < threshold);
    return value % bound;
}

static int call_random(pphp_state *state, const pstring *name,
                       const pvalue *arguments, size_t count, pvalue *result) {
    pphp_int minimum = 0;
    pphp_int maximum = INT32_MAX;
    uint64_t span;
    uint32_t offset;
    if (name_is(name, "srand")) {
        if (count > 1U || (count == 1U && arguments[0].type != PT_INT)) {
            return invalid_arguments(state, name);
        }
        state->random_state = count == 0U ? hal_random()
                                          : (uint32_t)arguments[0].as.i;
        if (state->random_state == 0U) state->random_state = 1U;
        *result = pv_null();
        return 1;
    }
    if (name_is(name, "random_int")) {
        if (count != 2U || arguments[0].type != PT_INT ||
            arguments[1].type != PT_INT) return invalid_arguments(state, name);
        minimum = arguments[0].as.i;
        maximum = arguments[1].as.i;
    } else if (count == 2U && arguments[0].type == PT_INT &&
               arguments[1].type == PT_INT) {
        minimum = arguments[0].as.i;
        maximum = arguments[1].as.i;
    } else if (count != 0U) {
        return invalid_arguments(state, name);
    }
    if (minimum > maximum) return invalid_arguments(state, name);
    span = (uint64_t)((int64_t)maximum - (int64_t)minimum) + 1U;
    offset = span > UINT32_MAX ? next_random(state)
                              : bounded_random(state, (uint32_t)span);
    *result = pv_int((pphp_int)((int64_t)minimum + (int64_t)offset));
    return 1;
}

static void split_timestamp(int64_t timestamp, int64_t *days,
                            unsigned *seconds) {
    int64_t quotient = timestamp / INT64_C(86400);
    int64_t remainder = timestamp % INT64_C(86400);
    if (remainder < 0) {
        remainder += INT64_C(86400);
        quotient--;
    }
    *days = quotient;
    *seconds = (unsigned)remainder;
}

/* Inverse of days-from-civil, with day zero at 1970-01-01. */
static void civil_from_days(int64_t days, int *year, unsigned *month,
                            unsigned *day) {
    int64_t era;
    unsigned day_of_era;
    unsigned year_of_era;
    int calculated_year;
    unsigned day_of_year;
    unsigned month_prime;
    days += INT64_C(719468);
    era = (days >= 0 ? days : days - INT64_C(146096)) / INT64_C(146097);
    day_of_era = (unsigned)(days - era * INT64_C(146097));
    year_of_era = (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
                   day_of_era / 146096U) / 365U;
    calculated_year = (int)year_of_era + (int)(era * 400);
    day_of_year = day_of_era -
                  (365U * year_of_era + year_of_era / 4U -
                   year_of_era / 100U);
    month_prime = (5U * day_of_year + 2U) / 153U;
    *day = day_of_year - (153U * month_prime + 2U) / 5U + 1U;
    *month = month_prime < 10U ? month_prime + 3U : month_prime - 9U;
    calculated_year += *month <= 2U;
    *year = calculated_year;
}

static void timestamp_fields(int64_t timestamp, utc_fields *fields) {
    int64_t days;
    int weekday;
    unsigned seconds;
    split_timestamp(timestamp, &days, &seconds);
    civil_from_days(days, &fields->year, &fields->month, &fields->day);
    fields->hour = seconds / 3600U;
    fields->minute = (seconds % 3600U) / 60U;
    fields->second = seconds % 60U;
    weekday = (int)((days + 4) % 7);
    if (weekday < 0) weekday += 7;
    fields->weekday = (unsigned)weekday;
    fields->iso_weekday = fields->weekday == 0U ? 7U : fields->weekday;
}

static int append_bytes(char *output, size_t capacity, size_t *length,
                        const char *bytes, size_t count) {
    if (count > capacity - *length) return 0;
    memcpy(output + *length, bytes, count);
    *length += count;
    return 1;
}

static int append_number(char *output, size_t capacity, size_t *length,
                         int number, unsigned width) {
    char buffer[24];
    int count = snprintf(buffer, sizeof(buffer), "%0*d", (int)width, number);
    return count >= 0 && append_bytes(output, capacity, length, buffer,
                                      (size_t)count);
}

static int call_date(pphp_state *state, const pstring *name,
                     const pvalue *arguments, size_t count, pvalue *result) {
    static const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed",
                                     "Thu", "Fri", "Sat"};
    const pstring *format;
    utc_fields fields;
    int64_t timestamp;
    size_t capacity;
    size_t output_length = 0U;
    size_t i;
    char *output;
    pstring *string;
    if (count < 1U || count > 2U || arguments[0].type != PT_STRING ||
        (count == 2U && arguments[1].type != PT_INT &&
         arguments[1].type != PT_NULL)) return invalid_arguments(state, name);
    format = (const pstring *)arguments[0].as.gc;
    timestamp = count == 2U && arguments[1].type == PT_INT
                    ? (int64_t)arguments[1].as.i
                    : (int64_t)(hal_time_us() / UINT64_C(1000000));
    timestamp_fields(timestamp, &fields);
    if (format->length > (PPHP_STR_MAX - 1U) / 12U) {
        pphp_runtime_error(state, 0U, "date() result exceeds maximum length");
        return -1;
    }
    capacity = format->length * 12U + 1U;
    output = pphp_alloc(capacity);
    if (output == NULL) {
        pphp_runtime_error(state, 0U, "out of memory formatting date");
        return -1;
    }
    for (i = 0U; i < format->length; i++) {
        char specifier = ps_data(format)[i];
        if (specifier == '\\' && i + 1U < format->length) {
            specifier = ps_data(format)[++i];
            if (!append_bytes(output, capacity, &output_length, &specifier, 1U))
                goto too_long;
        } else if (specifier == 'Y') {
            if (!append_number(output, capacity, &output_length, fields.year, 4U))
                goto too_long;
        } else if (specifier == 'y') {
            int year = fields.year % 100;
            if (year < 0) year = -year;
            if (!append_number(output, capacity, &output_length, year, 2U))
                goto too_long;
        } else if (specifier == 'm' || specifier == 'n') {
            if (!append_number(output, capacity, &output_length,
                               (int)fields.month, specifier == 'm' ? 2U : 1U))
                goto too_long;
        } else if (specifier == 'd' || specifier == 'j') {
            if (!append_number(output, capacity, &output_length,
                               (int)fields.day, specifier == 'd' ? 2U : 1U))
                goto too_long;
        } else if (specifier == 'H' || specifier == 'G') {
            if (!append_number(output, capacity, &output_length,
                               (int)fields.hour, specifier == 'H' ? 2U : 1U))
                goto too_long;
        } else if (specifier == 'i') {
            if (!append_number(output, capacity, &output_length,
                               (int)fields.minute, 2U)) goto too_long;
        } else if (specifier == 's') {
            if (!append_number(output, capacity, &output_length,
                               (int)fields.second, 2U)) goto too_long;
        } else if (specifier == 'D') {
            if (!append_bytes(output, capacity, &output_length,
                              weekdays[fields.weekday], 3U)) goto too_long;
        } else if (specifier == 'N' || specifier == 'w') {
            unsigned value = specifier == 'N' ? fields.iso_weekday
                                               : fields.weekday;
            if (!append_number(output, capacity, &output_length, (int)value, 1U))
                goto too_long;
        } else if (!append_bytes(output, capacity, &output_length,
                                 &specifier, 1U)) {
            goto too_long;
        }
    }
    string = ps_new(output, output_length);
    pphp_free(output);
    if (string == NULL) {
        pphp_runtime_error(state, 0U, "out of memory returning date");
        return -1;
    }
    *result = pv_heap(PT_STRING, &string->header);
    return 1;
too_long:
    pphp_free(output);
    pphp_runtime_error(state, 0U, "date() result exceeds maximum length");
    return -1;
}

static int call_clock(pphp_state *state, const pstring *name,
                      const pvalue *arguments, size_t count, pvalue *result) {
    uint64_t microseconds = hal_time_us();
    if (name_is(name, "time")) {
        if (count != 0U) return invalid_arguments(state, name);
#if PPHP_INT64
        *result = pv_int((pphp_int)(microseconds / UINT64_C(1000000)));
#else
#if PPHP_ENABLE_FLOAT
        if (microseconds / UINT64_C(1000000) > (uint64_t)INT32_MAX) {
            *result = pv_float((pphp_float)(microseconds / UINT64_C(1000000)));
        } else {
            *result = pv_int((pphp_int)(microseconds / UINT64_C(1000000)));
        }
#else
        *result = pv_int((pphp_int)(microseconds / UINT64_C(1000000)));
#endif
#endif
        return 1;
    }
    if (name_is(name, "microtime")) {
        int as_float = 1;
        if (count > 1U) return invalid_arguments(state, name);
        if (count == 1U) as_float = pv_is_truthy(arguments[0]);
        if (as_float) {
#if PPHP_ENABLE_FLOAT
            *result = pv_float((pphp_float)microseconds /
                               (pphp_float)1000000);
#else
            pphp_runtime_error(state, 0U, "float support disabled");
            return -1;
#endif
        } else {
            char buffer[48];
            int length = snprintf(buffer, sizeof(buffer), "0.%06llu %llu",
                                  (unsigned long long)(microseconds % UINT64_C(1000000)),
                                  (unsigned long long)(microseconds / UINT64_C(1000000)));
            pstring *string = length < 0 ? NULL
                                        : ps_new(buffer, (size_t)length);
            if (string == NULL) {
                pphp_runtime_error(state, 0U,
                                   "out of memory returning microtime");
                return -1;
            }
            *result = pv_heap(PT_STRING, &string->header);
        }
        return 1;
    }
    if (name_is(name, "hrtime")) {
        int as_number = 0;
        uint64_t nanoseconds = microseconds * UINT64_C(1000);
        if (count > 1U) return invalid_arguments(state, name);
        if (count == 1U) as_number = pv_is_truthy(arguments[0]);
        if (as_number) {
#if PPHP_INT64
            *result = pv_int((pphp_int)nanoseconds);
#else
#if PPHP_ENABLE_FLOAT
            *result = pv_float((pphp_float)nanoseconds);
#else
            pphp_runtime_error(state, 0U, "hrtime value exceeds integer range");
            return -1;
#endif
#endif
        } else {
            parray *array = pa_new(2U);
            pvalue seconds;
            if (array == NULL) goto clock_oom;
#if PPHP_INT64
            seconds = pv_int((pphp_int)(nanoseconds / UINT64_C(1000000000)));
#else
#if PPHP_ENABLE_FLOAT
            seconds = nanoseconds / UINT64_C(1000000000) > (uint64_t)INT32_MAX
                          ? pv_float((pphp_float)(nanoseconds /
                                                  UINT64_C(1000000000)))
                          : pv_int((pphp_int)(nanoseconds /
                                              UINT64_C(1000000000)));
#else
            seconds = pv_int((pphp_int)(nanoseconds / UINT64_C(1000000000)));
#endif
#endif
            if (!pa_push(array, seconds) ||
                !pa_push(array, pv_int((pphp_int)(nanoseconds %
                                                  UINT64_C(1000000000))))) {
                pv_release(pv_heap(PT_ARRAY, &array->header));
                goto clock_oom;
            }
            *result = pv_heap(PT_ARRAY, &array->header);
        }
        return 1;
    }
    return 0;
clock_oom:
    pphp_runtime_error(state, 0U, "out of memory returning clock value");
    return -1;
}

static int call_sleep(pphp_state *state, const pstring *name,
                      const pvalue *arguments, size_t count, pvalue *result) {
    uint64_t milliseconds;
    if (count != 1U || arguments[0].type != PT_INT || arguments[0].as.i < 0) {
        return invalid_arguments(state, name);
    }
    milliseconds = name_is(name, "sleep")
                       ? (uint64_t)arguments[0].as.i * UINT64_C(1000)
                       : ((uint64_t)arguments[0].as.i + UINT64_C(999)) /
                             UINT64_C(1000);
    while (milliseconds > UINT32_MAX) {
        hal_sleep_ms(UINT32_MAX);
        milliseconds -= UINT32_MAX;
    }
    hal_sleep_ms((uint32_t)milliseconds);
    *result = name_is(name, "sleep") ? pv_int(0) : pv_null();
    return 1;
}

static int call_system(pphp_state *state, const pstring *name,
                       const pvalue *arguments, size_t count, pvalue *result) {
    if (name_is(name, "memory_get_usage")) {
        pphp_pool_stats stats;
        if (count > 1U) return invalid_arguments(state, name);
        stats = pphp_pool_get_stats();
        *result = pv_int((pphp_int)stats.used);
        return 1;
    }
    if (name_is(name, "gc_collect_cycles")) {
        if (count != 0U) return invalid_arguments(state, name);
        *result = pv_int((pphp_int)pphp_gc_collect(state));
        return 1;
    }
    if (name_is(name, "error_log")) {
        pstring *message;
        if (count != 1U) return invalid_arguments(state, name);
        message = pv_to_string(arguments[0]);
        if (message == NULL) return invalid_arguments(state, name);
        hal_console_write(ps_data(message), message->length);
        hal_console_write("\n", 1U);
        ps_destroy(message);
        *result = pv_bool(1);
        return 1;
    }
    if (name_is(name, "exit") || name_is(name, "die")) {
        if (count > 1U) return invalid_arguments(state, name);
        state->exit_status = 0;
        if (count == 1U && arguments[0].type == PT_INT) {
            state->exit_status = (int)(arguments[0].as.i & 0xff);
        } else if (count == 1U) {
            pstring *message = pv_to_string(arguments[0]);
            if (message == NULL) return invalid_arguments(state, name);
            pphp_output(state, ps_data(message), message->length);
            ps_destroy(message);
        }
        state->exit_requested = 1;
        *result = pv_null();
        return 1;
    }
    return 0;
}

int pphp_call_system_builtin(pphp_state *state, const pstring *name,
                             const pvalue *arguments, size_t count,
                             pvalue *result) {
    int handled;
    if (name_is(name, "rand") || name_is(name, "mt_rand") ||
        name_is(name, "random_int") || name_is(name, "srand")) {
        return call_random(state, name, arguments, count, result);
    }
    if (name_is(name, "time") || name_is(name, "microtime") ||
        name_is(name, "hrtime")) {
        return call_clock(state, name, arguments, count, result);
    }
    if (name_is(name, "sleep") || name_is(name, "usleep")) {
        return call_sleep(state, name, arguments, count, result);
    }
    if (name_is(name, "date")) return call_date(state, name, arguments, count,
                                                 result);
    handled = call_system(state, name, arguments, count, result);
    return handled;
}
