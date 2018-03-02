/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/mman.h>

#include <limits.h>
#include <stdalign.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ldcache.h"
#include "utils.h"
#include "xfuncs.h"

/* See glibc dl-cache.c/h and ldconfig.c/h for the format definition. */

#define MAGIC_LIBC5       "ld.so-1.7.0"
#define MAGIC_LIBC6       "glibc-ld.so.cache"
#define MAGIC_VERSION     "1.1"
#define MAGIC_LIBC5_LEN   (sizeof(MAGIC_LIBC5) - 1)
#define MAGIC_LIBC6_LEN   (sizeof(MAGIC_LIBC6) - 1)
#define MAGIC_VERSION_LEN (sizeof(MAGIC_VERSION) - 1)

struct entry_libc5 {
        int32_t flags;
        uint32_t key;
        uint32_t value;
};

struct header_libc5 {
        char magic[MAGIC_LIBC5_LEN];
        uint32_t nlibs;
        struct entry_libc5 libs[];
};

struct entry_libc6 {
        int32_t flags;
        uint32_t key;
        uint32_t value;
        uint32_t osversion;
        uint64_t hwcap;
};

struct header_libc6 {
        char magic[MAGIC_LIBC6_LEN];
        char version[MAGIC_VERSION_LEN];
        uint32_t nlibs;
        uint32_t table_size;
        uint32_t unused[5];
        struct entry_libc6 libs[];
};

void
ldcache_init(struct ldcache *ctx, struct error *err, const char *path)
{
        *ctx = (struct ldcache){err, path, NULL, NULL, 0};
}

int
ldcache_open(struct ldcache *ctx)
{
        struct header_libc5 *h5;
        struct header_libc6 *h6;
        size_t padding;

        ctx->addr = ctx->ptr = file_map(ctx->err, ctx->path, &ctx->size);
        if (ctx->addr == NULL)
                return (-1);

        h5 = (struct header_libc5 *)ctx->ptr;
        if (ctx->size <= sizeof(*h5))
                goto fail;
        if (!strncmp(h5->magic, MAGIC_LIBC5, MAGIC_LIBC5_LEN)) {
                /* Do not support the old libc5 format, skip these entries. */
                ctx->ptr = h5->libs + h5->nlibs;
                padding = (-(uintptr_t)ctx->ptr) & (__alignof__(struct header_libc6) - 1);
                ctx->ptr = (char *)ctx->ptr + padding; /* align on header_libc6 boundary */
        }

        h6 = (struct header_libc6 *)ctx->ptr;
        if ((char *)ctx->addr + ctx->size - (char *)ctx->ptr <= (ptrdiff_t)sizeof(*h6))
                goto fail;
        if (strncmp(h6->magic, MAGIC_LIBC6, MAGIC_LIBC6_LEN) ||
            strncmp(h6->version, MAGIC_VERSION, MAGIC_VERSION_LEN))
                goto fail;

        return (0);

 fail:
        error_setx(ctx->err, "unsupported file format: %s", ctx->path);
        file_unmap(NULL, ctx->path, ctx->addr, ctx->size);
        return (-1);
}

int
ldcache_close(struct ldcache *ctx)
{
        if (file_unmap(ctx->err, ctx->path, ctx->addr, ctx->size) < 0)
                return (-1);

        ctx->addr = NULL;
        ctx->ptr = NULL;
        ctx->size = 0;
        return (0);
}

int
ldcache_resolve(struct ldcache *ctx, uint32_t arch, const char *root, const char * const libs[],
    char *paths[], size_t size, ldcache_select_fn select, void *select_ctx)
{
        char path[PATH_MAX];
        struct header_libc6 *h;
        int override;

        h = (struct header_libc6 *)ctx->ptr;
        memset(paths, 0, size * sizeof(*paths));

        for (uint32_t i = 0; i < h->nlibs; ++i) {
                int32_t flags = h->libs[i].flags;
                char *key = (char *)ctx->ptr + h->libs[i].key;
                char *value = (char *)ctx->ptr + h->libs[i].value;

                if (!(flags & LD_ELF) || (flags & LD_ARCH_MASK) != arch)
                        continue;

                for (size_t j = 0; j < size; ++j) {
                        if (!str_has_prefix(key, libs[j]))
                                continue;
                        if (path_resolve(ctx->err, path, root, value) < 0)
                                return (-1);
                        if (paths[j] != NULL && str_equal(paths[j], path))
                                continue;
                        if ((override = select(ctx->err, select_ctx, root, paths[j], path)) < 0)
                                return (-1);
                        if (override) {
                                free(paths[j]);
                                paths[j] = xstrdup(ctx->err, path);
                                if (paths[j] == NULL)
                                        return (-1);
                        }
                        break;
                }
        }
        return (0);
}
