#include "cbase.h"

#if TESTING_cbase_main_separate_object
#define CBASE_IMPLEMENT
#include "cbase.h"

int main(void) {
    error("Works.\n");
    exit(EXIT_SUCCESS);
}

#endif
