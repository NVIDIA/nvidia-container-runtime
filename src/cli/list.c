/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <alloca.h>
#include <err.h>
#include <stdio.h>

#include "cli.h"

static error_t list_parser(int, char *, struct argp_state *);

const struct argp list_usage = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"info", 'i', NULL, 0, "List driver version information", -1},
                {"device", 'd', "ID", 0, "Device UUID(s) or index(es) to list", -1},
                {"compute", 'c', NULL, 0, "List compute components", -1},
                {"utility", 'u', NULL, 0, "List utility components", -1},
                {"video", 'v', NULL, 0, "List video components", -1},
                {"graphics", 'g', NULL, 0, "List graphics components", -1},
                {"compat32", 0x80, NULL, 0, "List 32bits components", -1},
                {0},
        },
        list_parser,
        NULL,
        "Query the host driver and list the components required in order to configure a container with GPU support.",
        NULL,
        NULL,
        NULL,
};

static error_t
list_parser(int key, char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;
        struct error err = {0};

        switch (key) {
        case 'i':
                ctx->list_info = true;
                break;
        case 'd':
                if (strjoin(&err, &ctx->devices, arg, ",") < 0)
                        goto fatal;
                break;
        case 'c':
                if (strjoin(&err, &ctx->container_flags, "compute", " ") < 0)
                        goto fatal;
                break;
        case 'u':
                if (strjoin(&err, &ctx->container_flags, "utility", " ") < 0)
                        goto fatal;
                break;
        case 'v':
                if (strjoin(&err, &ctx->container_flags, "video", " ") < 0)
                        goto fatal;
                break;
        case 'g':
                if (strjoin(&err, &ctx->container_flags, "graphics", " ") < 0)
                        goto fatal;
                break;
        case 0x80:
                if (strjoin(&err, &ctx->container_flags, "compat32", " ") < 0)
                        goto fatal;
                break;
        default:
                return (ARGP_ERR_UNKNOWN);
        }
        return (0);

 fatal:
        errx(EXIT_FAILURE, "input error: %s", err.msg);
        return (0);
}

int
list_command(const struct context *ctx)
{
        bool run_as_root;
        struct nvc_context *nvc = NULL;
        struct nvc_config *nvc_cfg = NULL;
        struct nvc_driver_info *drv = NULL;
        struct nvc_device_info *dev = NULL;
        const struct nvc_device **gpus = NULL;
        struct error err = {0};
        int rv = EXIT_FAILURE;

        run_as_root = (geteuid() == 0);
        if (!run_as_root && ctx->load_kmods) {
                warnx("requires root privileges");
                return (rv);
        }
        if (run_as_root) {
                if (perm_set_capabilities(&err, CAP_PERMITTED, permitted_caps, nitems(permitted_caps)) < 0 ||
                    perm_set_capabilities(&err, CAP_INHERITABLE, inherited_caps, nitems(inherited_caps)) < 0 ||
                    perm_drop_bounds(&err) < 0) {
                        warnx("permission error: %s", err.msg);
                        return (rv);
                }
        } else {
                if (perm_set_capabilities(&err, CAP_PERMITTED, NULL, 0) < 0) {
                        warnx("permission error: %s", err.msg);
                        return (rv);
                }
        }

        /* Initialize the library context. */
        int c = ctx->load_kmods ? CAPS_INIT_KMODS : CAPS_INIT;
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[c], effective_caps_size(c)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((nvc = nvc_context_new()) == NULL ||
            (nvc_cfg = nvc_config_new()) == NULL) {
                warn("memory allocation failed");
                goto fail;
        }
        nvc_cfg->uid = (!run_as_root && ctx->uid == (uid_t)-1) ? geteuid() : ctx->uid;
        nvc_cfg->gid = (!run_as_root && ctx->gid == (gid_t)-1) ? getegid() : ctx->gid;
        if (nvc_init(nvc, nvc_cfg, ctx->init_flags) < 0) {
                warnx("initialization error: %s", nvc_error(nvc));
                goto fail;
        }

        /* Query the driver and device information. */
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_INFO], effective_caps_size(CAPS_INFO)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((drv = nvc_driver_info_new(nvc, ctx->driver_flags)) == NULL ||
            (dev = nvc_device_info_new(nvc, ctx->device_flags)) == NULL) {
                warnx("detection error: %s", nvc_error(nvc));
                goto fail;
        }

        /* List the driver information. */
        if (ctx->list_info) {
                printf("NVRM version: %s\n", drv->nvrm_version);
                printf("CUDA version: %s\n", drv->cuda_version);
        }

        /* List the visible GPU devices. */
        if (dev->ngpus > 0) {
                gpus = alloca(dev->ngpus * sizeof(*gpus));
                memset(gpus, 0, dev->ngpus * sizeof(*gpus));
                if (select_devices(&err, ctx->devices, gpus, dev->gpus, dev->ngpus) < 0) {
                        warnx("device error: %s", err.msg);
                        goto fail;
                }
        }
        if (ctx->devices != NULL) {
                for (size_t i = 0; i < drv->ndevs; ++i)
                        printf("%s\n", drv->devs[i].path);
                for (size_t i = 0; i < dev->ngpus; ++i) {
                        if (gpus[i] != NULL)
                                printf("%s\n", gpus[i]->node.path);
                }
        }

        /* List the driver components */
        for (size_t i = 0; i < drv->nbins; ++i)
                printf("%s\n", drv->bins[i]);
        for (size_t i = 0; i < drv->nlibs; ++i)
                printf("%s\n", drv->libs[i]);
        for (size_t i = 0; i < drv->nlibs32; ++i)
                printf("%s\n", drv->libs32[i]);
        for (size_t i = 0; i < drv->nipcs; ++i)
                printf("%s\n", drv->ipcs[i]);

        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_SHUTDOWN], effective_caps_size(CAPS_SHUTDOWN)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        rv = EXIT_SUCCESS;
 fail:
        nvc_shutdown(nvc);
        nvc_device_info_free(dev);
        nvc_driver_info_free(drv);
        nvc_config_free(nvc_cfg);
        nvc_context_free(nvc);
        error_reset(&err);
        return (rv);
}
