/* This file is part of clipsim. */
/* Copyright (C) 2022 Lucas Mior */

/* This program is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

#include <stdbool.h>
#include "clipsim.h"

#ifndef UTIL_H
#define UTIL_H

void *xalloc(void *, size_t);
void *xcalloc(size_t, size_t);
bool estrtol(int32 *, char *, int);
void segv_handler(int) __attribute__((noreturn));
void int_handler(int) __attribute__((noreturn));
void util_close(File *);
bool util_open(File *, int);

#endif /* UTIL_H */
