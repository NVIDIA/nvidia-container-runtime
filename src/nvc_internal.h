/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_NVC_INTERNAL_H
#define HEADER_NVC_INTERNAL_H

#include <sys/capability.h>
#include <sys/types.h>

#include <paths.h>
#include <stdbool.h>
#include <stdint.h>

#pragma GCC visibility push(default)
#include "nvc.h"
#pragma GCC visibility pop

#include "common.h"
#include "driver.h"
#include "error.h"
#include "ldcache.h"
#include "utils.h"

#define NV_DEVICE_MAJOR          195
#define NV_CTL_DEVICE_MINOR      255
#define NV_DEVICE_PATH           _PATH_DEV "nvidia%d"
#define NV_CTL_DEVICE_PATH       _PATH_DEV "nvidiactl"
#define NV_UVM_DEVICE_PATH       _PATH_DEV "nvidia-uvm"
#define NV_UVM_TOOLS_DEVICE_PATH _PATH_DEV "nvidia-uvm-tools"
#define NV_PERSISTENCED_SOCKET   _PATH_VARRUN "nvidia-persistenced/socket"
#define NV_MPS_PIPE_DIR          _PATH_TMP "nvidia-mps"
#define NV_PROC_DRIVER           "/proc/driver/nvidia"
#define NV_UVM_PROC_DRIVER       "/proc/driver/nvidia-uvm"

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

enum {
        CAPS_INIT,
        CAPS_INIT_KMODS,
        CAPS_SHUTDOWN,
        CAPS_CONTAINER,
        CAPS_INFO,
        CAPS_MOUNT,
        CAPS_LDCACHE,
};

static const cap_value_t permitted_caps[] = {
        CAP_CHOWN,           /* kmods */
        CAP_DAC_OVERRIDE,    /* rhel, cgroups */
        CAP_DAC_READ_SEARCH, /* userns */
        CAP_FOWNER,          /* kmods */
        CAP_KILL,            /* privsep */
        CAP_MKNOD,           /* kmods */
        CAP_SETGID,          /* privsep, userns */
        CAP_SETPCAP,         /* bounds, userns */
        CAP_SETUID,          /* privsep, userns */
        CAP_SYS_ADMIN,       /* setns, mount */
        CAP_SYS_CHROOT,      /* setns, chroot */
        CAP_SYS_PTRACE,      /* procns */
};

static const cap_value_t effective_caps[][nitems(permitted_caps) + 1] = {
        [CAPS_INIT]       = {CAP_KILL, CAP_SETGID, CAP_SETUID, -1},
        [CAPS_INIT_KMODS] = {CAP_CHOWN, CAP_FOWNER, CAP_MKNOD, CAP_KILL, CAP_SETGID, CAP_SETUID, -1},
        [CAPS_SHUTDOWN]   = {CAP_KILL, -1},
        [CAPS_CONTAINER]  = {CAP_DAC_READ_SEARCH, -1},
        [CAPS_INFO]       = {-1},
        [CAPS_MOUNT]      = {CAP_DAC_READ_SEARCH, CAP_SETGID, CAP_SETUID, CAP_SYS_ADMIN,
                             CAP_SYS_CHROOT, CAP_SYS_PTRACE, -1},
        [CAPS_LDCACHE]    = {CAP_DAC_READ_SEARCH, CAP_SETGID, CAP_SETPCAP, CAP_SETUID,
                             CAP_SYS_ADMIN, CAP_SYS_CHROOT, CAP_SYS_PTRACE, -1},
};

static const cap_value_t inherited_caps[] = {
        CAP_DAC_OVERRIDE,
        CAP_SYS_MODULE,
};

static inline size_t
effective_caps_size(int idx)
{
        size_t i;

        for (i = 0; i < nitems(*effective_caps); ++i) {
            if (effective_caps[idx][i] == -1)
                break;
        }
        return (i);
}

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

void nvc_entrypoint(void);

#endif /* HEADER_NVC_INTERNAL_H */
