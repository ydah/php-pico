#ifndef PPHP_FLOAT_MATH_H
#define PPHP_FLOAT_MATH_H

#include "pphp/pphp_config.h"

#if PPHP_ENABLE_FLOAT
#include <math.h>
#if PPHP_USE_DOUBLE
#define PPHP_FLOAT_MATH(function) function
#else
#define PPHP_FLOAT_MATH_NAME(function) function##f
#define PPHP_FLOAT_MATH(function) PPHP_FLOAT_MATH_NAME(function)
#endif
#endif

#endif
