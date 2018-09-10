/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <rpc/rpc.h>

#include <dlfcn.h>
#include <errno.h>

#include <libelf.h>

#include "cuda.h"
#include "nvml.h"

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
        const char *errmsg;
        va_list ap;
        int rv;

        switch (errcode) {
        case RPC_CANTSEND:
        case RPC_CANTRECV:
        case RPC_CANTENCODEARGS:
        case RPC_CANTDECODERES:
        case RPC_CANTDECODEARGS:
                errmsg = "failed to process request";
                break;
        case RPC_VERSMISMATCH:
        case RPC_PROGUNAVAIL:
        case RPC_PROGVERSMISMATCH:
        case RPC_PROCUNAVAIL:
        case RPC_UNKNOWNHOST:
        case RPC_UNKNOWNPROTO:
        case RPC_PMAPFAILURE:
        case RPC_PROGNOTREGISTERED:
                errmsg = "failed to perform handshake";
                break;
        default:
                errmsg = clnt_sperrno((enum clnt_stat)errcode);
                if (!strncmp(errmsg, "RPC: ", 5))
                        errmsg += 5;
                break;
        }
        va_start(ap, fmt);
        rv = error_vset(err, errcode, errmsg, fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_set_dl(struct error *err, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = error_vset(err, ELIBACC, dlerror(), fmt, ap);
        va_end(ap);
        return (rv);
}
