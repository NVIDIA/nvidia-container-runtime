/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/sysmacros.h>

#include <alloca.h>
#include <err.h>
#include <stdio.h>

#include "cli.h"

static error_t info_parser(int, char *, struct argp_state *);

const struct argp info_usage = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"csv", 0x80, NULL, 0, "Output in CSV format", -1},
                {0},
        },
        info_parser,
        NULL,
        "Query the driver and report its information as well as the devices it detected.",
        NULL,
        NULL,
        NULL,
};

static error_t
info_parser(int key, maybe_unused char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;

        switch (key) {
        case 0x80:
                ctx->csv_output = true;
                break;
        default:
                return (ARGP_ERR_UNKNOWN);
        }
        return (0);
}

int
info_command(const struct context *ctx)
{
        bool run_as_root;
        struct nvc_context *nvc = NULL;
        struct nvc_config *nvc_cfg = NULL;
        struct nvc_driver_info *drv = NULL;
        struct nvc_device_info *dev = NULL;
        struct error err = {0};
        int rv = EXIT_FAILURE;

        run_as_root = (geteuid() == 0);
        if (run_as_root) {
                if (perm_set_capabilities(&err, CAP_PERMITTED, pcaps, nitems(pcaps)) < 0 ||
                    perm_set_capabilities(&err, CAP_INHERITABLE, NULL, 0) < 0 ||
                    perm_set_bounds(&err, bcaps, nitems(bcaps)) < 0) {
                        warnx("permission error: %s", err.msg);
                        return (rv);
                }
        }

        /* Initialize the library context. */
        int c = ctx->load_kmods ? NVC_INIT_KMODS : NVC_INIT;
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[c], ecaps_size(c)) < 0) {
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
        nvc_cfg->root = ctx->root;
        nvc_cfg->ldcache = ctx->ldcache;
        if (nvc_init(nvc, nvc_cfg, ctx->init_flags) < 0) {
                warnx("initialization error: %s", nvc_error(nvc));
                goto fail;
        }

        /* Query the driver and device information. */
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_INFO], ecaps_size(NVC_INFO)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((drv = nvc_driver_info_new(nvc, NULL)) == NULL ||
            (dev = nvc_device_info_new(nvc, NULL)) == NULL) {
                warnx("detection error: %s", nvc_error(nvc));
                goto fail;
        }

        if (ctx->csv_output) {
                printf("NVRM version,CUDA version\n%s,%s\n", drv->nvrm_version, drv->cuda_version);
                printf("\nDevice Index,Device Minor,Model,Brand,GPU UUID,Bus Location,Architecture\n");
                for (size_t i = 0; i < dev->ngpus; ++i)
                        printf("%zu,%u,%s,%s,%s,%s,%s\n", i, minor(dev->gpus[i].node.id), dev->gpus[i].model, dev->gpus[i].brand,
                            dev->gpus[i].uuid, dev->gpus[i].busid, dev->gpus[i].arch);

        } else {
                printf("%-15s %s\n%-15s %s\n", "NVRM version:", drv->nvrm_version, "CUDA version:", drv->cuda_version);
                for (size_t i = 0; i < dev->ngpus; ++i)
                        printf("\n%-15s %zu\n%-15s %u\n%-15s %s\n%-15s %s\n%-15s %s\n%-15s %s\n%-15s %s\n",
                            "Device Index:", i, "Device Minor:", minor(dev->gpus[i].node.id), "Model:", dev->gpus[i].model, "Brand:",
                            dev->gpus[i].brand, "GPU UUID:", dev->gpus[i].uuid, "Bus Location:", dev->gpus[i].busid, "Architecture:", dev->gpus[i].arch);
        }

        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, ecaps[NVC_SHUTDOWN], ecaps_size(NVC_SHUTDOWN)) < 0) {
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
