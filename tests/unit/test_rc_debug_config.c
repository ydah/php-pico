#include "pphp/pphp_config.h"

typedef char rc_debug_is_boolean[
    PPHP_RC_DEBUG == 0 || PPHP_RC_DEBUG == 1 ? 1 : -1];

int main(void) {
    return PPHP_RC_DEBUG == 0 || PPHP_RC_DEBUG == 1 ? 0 : 1;
}
