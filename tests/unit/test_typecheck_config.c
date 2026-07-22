#include "pphp/pphp_config.h"

typedef char typecheck_is_boolean[
    PPHP_TYPECHECK == 0 || PPHP_TYPECHECK == 1 ? 1 : -1];

int main(void) {
    return PPHP_TYPECHECK == 0 || PPHP_TYPECHECK == 1 ? 0 : 1;
}
