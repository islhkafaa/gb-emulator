#include <stdio.h>
#include "gb.h"

int main(void) {
    GB gb = {0};

    if (!gb_init(&gb)) {
        fprintf(stderr, "gb_init failed\n");
        return 1;
    }

    gb_run(&gb);
    gb_quit(&gb);

    return 0;
}
