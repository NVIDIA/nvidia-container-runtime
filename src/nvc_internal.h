/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_NVC_INTERNAL_H
#define HEADER_NVC_INTERNAL_H

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

#define NV_DEVICE_MAJOR          195
#define NV_CTL_DEVICE_MINOR      255
#define NV_DEVICE_PATH           _PATH_DEV "nvidia%d"
#define NV_CTL_DEVICE_PATH       _PATH_DEV "nvidiactl"
#define NV_UVM_DEVICE_PATH       _PATH_DEV "nvidia-uvm"
#define NV_UVM_TOOLS_DEVICE_PATH _PATH_DEV "nvidia-uvm-tools"
#define NV_PERSISTENCED_SOCKET   _PATH_VARRUN "nvidia-persistenced/socket"
#define NV_MPS_PIPE_DIR          _PATH_TMP "nvidia-mps"

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

void nvc_entrypoint(void);

#endif /* HEADER_NVC_INTERNAL_H */
