/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is part of libnvidia-container.
 * If this file is being compiled and dynamically linked against libelf from the
 * elfutils package (usually characterized by the definition of the macro `WITH_LIBELF'),
 * the following license notice covers the use of said library along with the terms from
 * the COPYING and COPYING.LESSER files:
 *
 *  elfutils is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  elfutils is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received copies of the GNU General Public License and
 *  the GNU Lesser General Public License along with this program.  If
 *  not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEADER_ELFTOOL_H
#define HEADER_ELFTOOL_H

#include <stdint.h>

#include <libelf.h>

#include "error.h"

struct elftool {
    struct error *err;
    int fd;
    Elf *elf;
    const char *path;
};

void elftool_init(struct elftool *, struct error *);
int  elftool_open(struct elftool *, const char *);
void elftool_close(struct elftool *);
int  elftool_has_dependency(struct elftool *, const char *);
int  elftool_has_abi(struct elftool *, uint32_t [3]);

#endif /* HEADER_ELFTOOL_H */
