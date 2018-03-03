/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_LDCACHE_H
#define HEADER_LDCACHE_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"

struct ldcache {
        struct error *err;
        const char *path;
        void *addr;
        void *ptr;
        size_t size;
};

enum {
        LD_UNKNOWN                 = -1,

        LD_TYPE_MASK               = 0x00ff,
        LD_ELF                     = 0x0001,
        LD_ELF_LIBC5               = 0x0002,
        LD_ELF_LIBC6               = 0x0003,

        LD_ARCH_MASK               = 0xff00,
        LD_I386_LIB32              = 0x0000,
        LD_SPARC_LIB64             = 0x0100,
        LD_IA64_LIB64              = 0x0200,
        LD_X8664_LIB64             = 0x0300,
        LD_S390_LIB64              = 0x0400,
        LD_POWERPC_LIB64           = 0x0500,
        LD_MIPS64_LIBN32           = 0x0600,
        LD_MIPS64_LIBN64           = 0x0700,
        LD_X8664_LIBX32            = 0x0800,
        LD_ARM_LIBHF               = 0x0900,
        LD_AARCH64_LIB64           = 0x0a00,
        LD_ARM_LIBSF               = 0x0b00,
        LD_MIPS_LIB32_NAN2008      = 0x0c00,
        LD_MIPS64_LIBN32_NAN2008   = 0x0d00,
        LD_MIPS64_LIBN64_NAN2008   = 0x0e00,
};

typedef int (*ldcache_select_fn)(struct error *, void *, const char *, const char *, const char *);

void ldcache_init(struct ldcache *, struct error *, const char *);
int  ldcache_open(struct ldcache *);
int  ldcache_close(struct ldcache *);
int  ldcache_resolve(struct ldcache *, uint32_t, const char *, const char * const [],
    char *[], size_t, ldcache_select_fn, void *);

#endif /* HEADER_LDCACHE_H */
