#include "pphp/pphp.h"

#include <stdio.h>
#include <string.h>

static void print_usage(FILE *stream) {
    fprintf(stream, "Usage: php-pico [--version]\n");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("php-pico %s\n", PPHP_VERSION);
        return 0;
    }
    print_usage(argc == 1 ? stdout : stderr);
    return argc == 1 ? 0 : 2;
}

