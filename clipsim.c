// SPDX-License-Identifier: AGPL
// Copyright (c) 2026 Lucas Mior

#if !defined(CLIPSIM_C)
#define CLIPSIM_C

#include "cbase.h"
#include "clipsim.h"

#if defined(__INCLUDE_LEVEL__) && (__INCLUDE_LEVEL__ == 0)
#define TESTING_clipsim 1
#elif !defined(TESTING_clipsim)
#define TESTING_clipsim 0
#endif

void
util_close(File *file) {
    if (file->fd >= 0) {
        if (close(file->fd) < 0)
            fprintf(stderr, "Error closing %s: %s\n", file->name, strerror(errno));
        file->fd = -1;
    }
    if (file->file != NULL) {
        if (fclose(file->file) != 0)
            fprintf(stderr, "Error closing %s: %s\n", file->name, strerror(errno));
        file->file = NULL;
    }
    return;
}

void
reopen_magic(void) {
    if (magic) {
        magic_close(magic);
    }
    if ((magic = magic_open(MAGIC_MIME_TYPE)) == NULL) {
        error("Error in magic_open(MAGIC_MIME_TYPE): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (magic_load(magic, NULL) != 0) {
        error("Error in magic_load(): %s\n", magic_error(magic));
        exit(EXIT_FAILURE);
    }
    return;
}


#if TESTING_clipsim
#define CBASE_IMPLEMENT
#include "cbase.h"

int
main(void) {
	ASSERT(true);
	exit(EXIT_SUCCESS);
}

#endif /* TESTING_clipsim */

#endif /* CLIPSIM_C */
