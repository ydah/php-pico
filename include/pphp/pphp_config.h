#ifndef PPHP_CONFIG_H
#define PPHP_CONFIG_H

#include <stdint.h>

#ifndef PPHP_INT64
#define PPHP_INT64 0
#endif

#ifndef PPHP_USE_DOUBLE
#if defined(PPHP_HOST)
#define PPHP_USE_DOUBLE 1
#else
#define PPHP_USE_DOUBLE 0
#endif
#endif

#ifndef PPHP_ENABLE_FLOAT
#define PPHP_ENABLE_FLOAT 1
#endif

#ifndef PPHP_ENABLE_COMPILER
#define PPHP_ENABLE_COMPILER 1
#endif

#ifndef PPHP_ENABLE_CYCLE_GC
#define PPHP_ENABLE_CYCLE_GC 1
#endif

#ifndef PPHP_ENABLE_PGEMS
#define PPHP_ENABLE_PGEMS 1
#endif

#ifndef PPHP_LINE_INFO
#define PPHP_LINE_INFO 1
#endif

#ifndef PPHP_TYPECHECK
#define PPHP_TYPECHECK 0
#endif

#ifndef PPHP_VIS_CHECK
#define PPHP_VIS_CHECK 1
#endif

#ifndef PPHP_WARNINGS
#define PPHP_WARNINGS 1
#endif

#ifndef PPHP_STACK_SLOTS
#define PPHP_STACK_SLOTS 384
#endif

#ifndef PPHP_FRAME_MAX
#define PPHP_FRAME_MAX 48
#endif

#ifndef PPHP_STR_MAX
#define PPHP_STR_MAX 65535U
#endif

#ifndef PPHP_HEAP_SIZE
#define PPHP_HEAP_SIZE (128U * 1024U)
#endif

#ifndef PPHP_PARSE_DEPTH_MAX
#define PPHP_PARSE_DEPTH_MAX 64
#endif

#if PPHP_INT64
typedef int64_t pphp_int;
#else
typedef int32_t pphp_int;
#endif

#if PPHP_USE_DOUBLE
typedef double pphp_float;
#else
typedef float pphp_float;
#endif

#endif
