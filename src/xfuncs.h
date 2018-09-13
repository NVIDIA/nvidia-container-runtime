/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_XFUNCS_H
#define HEADER_XFUNCS_H

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline void xclose(int);
static inline int  xopen(struct error *, const char *, int);
static inline void *xcalloc(struct error *, size_t, size_t);
static inline int  xstat(struct error *, const char *, struct stat *);
static inline FILE *xfopen(struct error *, const char *, const char *);
static inline char *xstrdup(struct error *, const char *);
static inline int  xasprintf(struct error *, char **, const char *, ...)
    __attribute__((format(printf, 3, 4), nonnull(3)));
static inline int  xsnprintf(struct error *, char *, size_t, const char *, ...)
    __attribute__((format(printf, 4, 5), nonnull(4)));
static inline void *xdlopen(struct error *, const char *, int);
static inline int  xdlclose(struct error *, void *);
static inline int  xmount(struct error *, const char *, const char *,
    const char *, unsigned long, const void *);
static inline int  xglob(struct error *, const char *, int, int (*)(const char *, int), glob_t *);

#include "error.h"

static inline void
xclose(int fd)
{
        if (fd < 0)
                return;
        close(fd);
}

static inline int
xopen(struct error *err, const char *path, int flags)
{
        int rv;

        if ((rv = open(path, flags)) < 0)
                error_set(err, "open failed: %s", path);
        return (rv);
}

static inline void *
xcalloc(struct error *err, size_t nmemb, size_t size)
{
        void *p;

        if ((p = calloc(nmemb, size)) == NULL)
                error_set(err, "memory allocation failed");
        return (p);
}

static inline int
xstat(struct error *err, const char *path, struct stat *buf)
{
        int rv;

        if ((rv = stat(path, buf)) < 0)
                error_set(err, "stat failed: %s", path);
        return (rv);
}

static inline FILE *
xfopen(struct error *err, const char *path, const char *mode)
{
        FILE *f;

        if ((f = fopen(path, mode)) == NULL)
                error_set(err, "open failed: %s", path);
        return (f);
}

static inline char *
xstrdup(struct error *err, const char *s)
{
        char *p;

        if ((p = strdup(s)) == NULL)
                error_set(err, "memory allocation failed");
        return (p);
}

static inline int
xasprintf(struct error *err, char **strp, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        if ((rv = vasprintf(strp, fmt, ap)) < 0) {
            error_set(err, "memory allocation failed");
            *strp = NULL;
        }
        va_end(ap);
        return (rv);
}

static inline int
xsnprintf(struct error *err, char *str, size_t size, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        if ((rv = vsnprintf(str, size, fmt, ap)) < 0 || (size_t)rv >= size) {
            error_setx(err, "string formatting failed");
            rv = -1;
        }
        va_end(ap);
        return (rv);
}

static inline void *
xdlopen(struct error *err, const char *filename, int flags)
{
        void *h;

        if ((h = dlopen(filename, flags)) == NULL)
                error_set_dl(err, "load library failed");
        return (h);
}

static inline int
xdlclose(struct error *err, void *handle)
{
        if (handle == NULL)
                return (0);
        if (dlclose(handle) != 0) {
                error_set_dl(err, "unload library failed");
                return (-1);
        }
        return (0);
}

static inline int
xmount(struct error *err, const char *source, const char *target,
    const char *filesystemtype, unsigned long mountflags, const void *data)
{
        int rv;

        if ((rv = mount(source, target, filesystemtype, mountflags, data)) < 0)
                error_set(err, "mount operation failed: %s", (source != NULL && *source == '/') ? source : target);
        return (rv);
}

static inline int
xglob(struct error *err, const char *pattern, int flags, int (*errfn)(const char *, int), glob_t *pglob)
{
        int rv;

        rv = glob(pattern, flags, errfn, pglob);
        if (rv != 0 && rv != GLOB_NOMATCH && errno != ENOENT) {
            error_set(err, "glob search failed: %s", pattern);
            return (-1);
        }
        if (rv != 0) {
            pglob->gl_pathc = 0;
            pglob->gl_pathv = NULL;
        }
        return (0);
}

#endif /* HEADER_XFUNCS_H */
