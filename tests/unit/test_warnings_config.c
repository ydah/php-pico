#include "pphp/pphp_config.h"

int main(void) {
    return PPHP_WARNINGS == 0 || PPHP_WARNINGS == 1 ? 0 : 1;
}
