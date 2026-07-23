#include "pphp/pphp_config.h"

#if PPHP_ENABLE_COMPILER != 0 && PPHP_ENABLE_COMPILER != 1
#error "test requires a boolean PPHP_ENABLE_COMPILER"
#endif

int pphp_test_compiler_config(void) {
    return PPHP_ENABLE_COMPILER;
}
