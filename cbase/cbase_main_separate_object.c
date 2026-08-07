// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#include "cbase.h"

#if TESTING_cbase_main_separate_object
#define CBASE_IMPLEMENT 1
#include "cbase.h"

int main(void) {
    error("Works.\n");
    exit(EXIT_SUCCESS);
}

#endif
