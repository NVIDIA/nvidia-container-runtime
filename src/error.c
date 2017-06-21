/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <rpc/rpc.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include <cuda.h>
#include <libelf.h>
#include <nvml.h>

#include "error.h"
#include "utils.h"

static int error_vset(struct error *, int, const char *, const char *, va_list)
    __attribute__((format(printf, 4, 0), nonnull(4)));

void
error_reset(struct error *err)
{
        if (err == NULL)
                return;

        free(err->msg);
        err->msg = NULL;
        err->code = 0;
}

static int
error_vset(struct error *err, int errcode, const char *errmsg, const char *fmt, va_list ap)
{
        char *msg = NULL;
        int rv = -1;

        if (err == NULL)
                return (0);

        error_reset(err);
        err->code = errcode;

        if (vasprintf(&msg, fmt, ap) < 0) {
                msg = NULL;
                goto fail;
        }
        if (errmsg == NULL) {
                err->msg = msg;
                strlower(err->msg);
                return (0);
        }
        if (asprintf(&err->msg, "%s: %s", msg, errmsg) < 0) {
                err->msg = NULL;
                goto fail;
        }
        err->msg[strcspn(err->msg, "\n")] = '\0';
        strlower(err->msg);
        rv = 0;

 fail:
        free(msg);
        return (rv);
}

int
error_set(struct error *err, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = error_vset(err, errno, strerror(errno), fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_setx(struct error *err, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = error_vset(err, -1, NULL, fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_set_elf(struct error *err, const char *fmt, ...)
{
        int errcode;
        va_list ap;
        int rv;

        errcode = elf_errno();
        va_start(ap, fmt);
        rv = error_vset(err, errcode, elf_errmsg(errcode), fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_set_nvml(struct error *err, void *handle, int errcode, const char *fmt, ...)
{
        static union {void *ptr; char *(*fn)(nvmlReturn_t);} errfn;
        const char *errmsg = "unknown error";
        va_list ap;
        int rv;

        if (errfn.ptr == NULL) {
                dlerror();
                errfn.ptr = dlsym(handle, "nvmlErrorString");
        }
        if (errfn.ptr != NULL || dlerror() == NULL)
                errmsg = (*errfn.fn)((nvmlReturn_t)errcode);

        va_start(ap, fmt);
        rv = error_vset(err, errcode, errmsg, fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_set_cuda(struct error *err, void *handle, int errcode, const char *fmt, ...)
{
        static union {void *ptr; CUresult (*fn)(CUresult, const char **);} errfn;
        const char *errmsg = "unknown error";
        va_list ap;
        int rv;

        if (errfn.ptr == NULL) {
                dlerror();
                errfn.ptr = dlsym(handle, "cuGetErrorString");
        }
        if (errfn.ptr != NULL || dlerror() == NULL)
                (*errfn.fn)((CUresult)errcode, &errmsg);

        va_start(ap, fmt);
        rv = error_vset(err, errcode, errmsg, fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_set_rpc(struct error *err, int errcode, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = error_vset(err, errcode, clnt_sperrno((enum clnt_stat)errcode), fmt, ap);
        va_end(ap);
        return (rv);
}
