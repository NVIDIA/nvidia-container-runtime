/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <rpc/rpc.h>

#include <dlfcn.h>

#include <cuda.h>
#include <libelf.h>
#include <nvml.h>

#include "error.h"

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
