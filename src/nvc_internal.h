/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_NVC_INTERNAL_H
#define HEADER_NVC_INTERNAL_H

#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

#pragma GCC visibility push(default)
#include "nvc.h"
#pragma GCC visibility pop

#include "driver.h"
#include "error.h"
#include "ldcache.h"

#define PROC_PID                  "/proc/%ld"
#define PROC_SELF                 "/proc/self"
#define PROC_MOUNTS_PATH(proc)    proc "/mountinfo"
#define PROC_CGROUP_PATH(proc)    proc "/cgroup"
#define PROC_NS_PATH(proc)        proc "/ns/%s"
#define PROC_SETGROUPS_PATH(proc) proc "/setgroups"
#define PROC_LAST_CAP_PATH        "/proc/sys/kernel/cap_last_cap"

#define LDCACHE_PATH              "/etc/ld.so.cache"
#define LDCONFIG_PATH             "/sbin/ldconfig"
#define LDCONFIG_ALT_PATH         "/sbin/ldconfig.real"

#define LIB_DIR                   "/lib64"
#define USR_BIN_DIR               "/usr/bin"
#define USR_LIB_DIR               "/usr/lib64"
#define USR_LIB32_DIR             "/usr/lib32"
#define USR_LIB32_ALT_DIR         "/usr/lib"

#if defined(__x86_64__)
# define LIB_ARCH                 LD_X8664_LIB64
# define LIB32_ARCH               LD_I386_LIB32
# define USR_LIB_MULTIARCH_DIR    "/usr/lib/x86_64-linux-gnu"
# define USR_LIB32_MULTIARCH_DIR  "/usr/lib/i386-linux-gnu"
#else
# error "unsupported architecture"
#endif

struct nvc_context {
        bool initialized;
        struct error err;
        struct nvc_config cfg;
        int mnt_ns;
        struct driver drv;
};

struct nvc_container {
        int32_t flags;
        struct nvc_container_config cfg;
        uid_t uid;
        gid_t gid;
        char *mnt_ns;
        char *dev_cg;
};

static inline int
validate_context(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return (-1);
        if (!ctx->initialized) {
                error_setx(&ctx->err, "context uninitialized");
                return (-1);
        }
        return (0);
}

static inline int
validate_args(struct nvc_context *ctx, bool predicate)
{
        if (!predicate) {
                error_setx(&ctx->err, "invalid argument");
                return (-1);
        }
        return (0);
}

void nvc_entry(void);

#endif /* HEADER_NVC_INTERNAL_H */
