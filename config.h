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

#include "clipsim.h"

#ifndef CONFIG_H
#define CONFIG_H

/* Maximum size for a single clipboard Entry, in bytes.
 * In practice the maximum size will be smaller due to
 * X11's convoluted inner workings */
static const uint MAX_ENTRY_SIZE = 0xFFFF;

/* Digits for printing id of each entry */
static const uint PRINT_DIGITS = 3;

/* How many bytes from each entry are to be printed when
 * looking entire history */
static const size_t OUT_SIZE = 255;

#endif /* CONFIG_H */
