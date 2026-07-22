#ifndef PPHP_FLOAT_FORMAT_H
#define PPHP_FLOAT_FORMAT_H

#include "pphp/pphp_config.h"

#include <stddef.h>

#if PPHP_ENABLE_FLOAT && PPHP_USE_DOUBLE
#define PPHP_FLOAT_FORMAT_BUFFER_SIZE 384U
#else
#define PPHP_FLOAT_FORMAT_BUFFER_SIZE 128U
#endif

/*
 * Format a floating-point value without field padding.  Conversion is one of
 * 'f', 'e', or 'g'.  A negative precision selects the printf default of six.
 * The return value is the byte length, or -1 when the arguments or capacity
 * are invalid.
 */
#if PPHP_ENABLE_FLOAT
int pphp_format_float(char *buffer, size_t capacity, pphp_float value,
                      char conversion, int precision);
#endif

#endif
