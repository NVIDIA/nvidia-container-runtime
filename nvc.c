/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>

#include <pci-enum.h>
#include <nvidia-modprobe-utils.h>

#include "nvc_internal.h"

#include "driver.h"
#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

static void load_kernel_modules(void);

static const struct nvc_version version = {
        NVC_MAJOR,
        NVC_MINOR,
        NVC_REVISION,
        NVC_VERSION,
};

const struct nvc_version *
nvc_version(void)
{
        return (&version);
}

struct nvc_config *
nvc_config_new(void)
{
        struct nvc_config *cfg;

        if ((cfg = calloc(1, sizeof(*cfg))) == NULL)
                return (NULL);
        return (cfg);
}

void
nvc_config_free(struct nvc_config *cfg)
{
        if (cfg == NULL)
                return;
        free(cfg);
}

struct nvc_context *
nvc_context_new(void)
{
        struct nvc_context *ctx;

        if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
                return (NULL);
        return (ctx);
}

void
nvc_context_free(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return;
        error_reset(&ctx->err);
        free(ctx);
}

static void
load_kernel_modules(void)
{
        struct pci_id_match devs = {
                0x10de,        /* vendor (NVIDIA) */
                PCI_MATCH_ANY, /* device */
                PCI_MATCH_ANY, /* subvendor */
                PCI_MATCH_ANY, /* subdevice */
                0x0300,        /* class (display) */
                0xff00,        /* class mask (any subclass) */
                0,             /* match count */
        };

        if (pci_enum_match_id(&devs) != 0 || devs.num_matches == 0)
                log_warn("failed to detect NVIDIA devices");

        log_info("loading kernel module nvidia");
        if (nvidia_modprobe(0, -1) == 0)
                log_err("could not load kernel module nvidia");
        else {
                for (int i = 0; i < (int)devs.num_matches; ++i) {
                        if (nvidia_mknod(i, -1) == 0)
                                log_err("could not create kernel module device node");
                }
        }

        log_info("loading kernel module nvidia_uvm");
        if (nvidia_uvm_modprobe(0) == 0)
                log_err("could not load kernel module nvidia_uvm");
        else {
                if (nvidia_uvm_mknod(0) == 0)
                        log_err("could not create kernel module device node");
        }

        log_info("loading kernel module nvidia_modeset");
        if (nvidia_modeset_modprobe() == 0)
                log_err("could not load kernel module nvidia_modeset");
        else {
                if (nvidia_modeset_mknod() == 0)
                        log_err("could not create kernel module device node");
        }
}

int
nvc_init(struct nvc_context *ctx, const struct nvc_config *cfg, const char *opts)
{
        int32_t flags;
        char path[PATH_MAX];
        const char *ldcache;

        if (ctx == NULL)
                return (-1);
        if (ctx->initialized)
                return (0);
        if (cfg == NULL)
                cfg = &(struct nvc_config){0};
        if (opts == NULL)
                opts = default_library_opts;
        if ((flags = options_parse(&ctx->err, opts, library_opts, nitems(library_opts))) < 0)
                return (-1);

        log_open(getenv("NVC_DEBUG_FILE"));
        log_info("initializing library context (version=%s, build=%s)", NVC_VERSION, SCM_REVISION);

        if (flags & OPT_LOAD_KMODS)
                load_kernel_modules();

        memset(&ctx->cfg, 0, sizeof(ctx->cfg));
        ctx->mnt_ns = -1;

        ldcache = (cfg->ldcache != NULL) ? cfg->ldcache : LDCACHE_PATH;
        if ((ctx->cfg.ldcache = xrealpath(&ctx->err, ldcache, NULL)) == NULL)
                goto fail;
        log_info("using ldcache %s", ctx->cfg.ldcache);

        if (xsnprintf(&ctx->err, path, sizeof(path), PROC_NS_PATH(PROC_SELF), "mnt") < 0)
                goto fail;
        if ((ctx->mnt_ns = xopen(&ctx->err, path, O_RDONLY|O_CLOEXEC)) < 0)
                goto fail;
        if (driver_init(&ctx->drv, &ctx->err) < 0)
                goto fail;

        ctx->initialized = true;
        return (0);

 fail:
        free(ctx->cfg.ldcache);
        xclose(ctx->mnt_ns);
        return (-1);
}

int
nvc_shutdown(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return (-1);
        if (!ctx->initialized)
                return (0);

        log_info("shutting down library context");
        if (driver_shutdown(&ctx->drv) < 0)
                return (-1);
        free(ctx->cfg.ldcache);
        xclose(ctx->mnt_ns);

        memset(&ctx->cfg, 0, sizeof(ctx->cfg));
        ctx->mnt_ns = -1;

        log_close();
        ctx->initialized = false;
        return (0);
}

const char *
nvc_error(struct nvc_context *ctx)
{
        if (ctx == NULL)
                return (NULL);
        if (ctx->err.code != 0 && ctx->err.msg == NULL)
                return ("unknown error");
        return (ctx->err.msg);
}
