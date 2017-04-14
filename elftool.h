/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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
