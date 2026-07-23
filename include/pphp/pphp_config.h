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

#if PPHP_ENABLE_FLOAT != 0 && PPHP_ENABLE_FLOAT != 1
#error "PPHP_ENABLE_FLOAT must be 0 or 1"
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

#ifndef PPHP_TRACE
#define PPHP_TRACE 0
#endif

#ifndef PPHP_TYPECHECK
#define PPHP_TYPECHECK 0
#endif

#if PPHP_TYPECHECK != 0 && PPHP_TYPECHECK != 1
#error "PPHP_TYPECHECK must be 0 or 1"
#endif

#ifndef PPHP_RC_DEBUG
#define PPHP_RC_DEBUG 0
#endif

#if PPHP_RC_DEBUG != 0 && PPHP_RC_DEBUG != 1
#error "PPHP_RC_DEBUG must be 0 or 1"
#endif

#ifndef PPHP_VIS_CHECK
#define PPHP_VIS_CHECK 1
#endif

#ifndef PPHP_WARNINGS
#define PPHP_WARNINGS 1
#endif

#if PPHP_WARNINGS != 0 && PPHP_WARNINGS != 1
#error "PPHP_WARNINGS must be 0 or 1"
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

#ifndef PPHP_FLASH_FS_SIZE
#define PPHP_FLASH_FS_SIZE (1024U * 1024U)
#endif

#ifndef PPHP_PARSE_DEPTH_MAX
#define PPHP_PARSE_DEPTH_MAX 64
#endif

#if PPHP_INT64
typedef int64_t pphp_int;
#define PPHP_INT_MINIMUM INT64_MIN
#define PPHP_INT_MAXIMUM INT64_MAX
#else
typedef int32_t pphp_int;
#define PPHP_INT_MINIMUM INT32_MIN
#define PPHP_INT_MAXIMUM INT32_MAX
#endif

#if !PPHP_ENABLE_FLOAT
/* Integer-only builds keep the internal numeric API source-compatible
 * without introducing a floating-point C type or operation. */
typedef pphp_int pphp_float;
#elif PPHP_USE_DOUBLE
typedef double pphp_float;
#else
typedef float pphp_float;
#endif

#endif
