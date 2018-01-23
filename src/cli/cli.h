/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_CLI_H
#define HEADER_CLI_H

#include <argp.h>
#include <stdbool.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error_generic.h"

struct context;

struct command {
        const char *name;
        const struct argp *argp;
        int (*func)(const struct context *);
};

struct context {
        /* main */
        uid_t uid;
        gid_t gid;
        char *ldcache;
        bool load_kmods;
        char *init_flags;
        const struct command *command;

        /* info */
        bool csv_output;

        /* configure */
        pid_t pid;
        char *rootfs;
        char *reqs[32];
        size_t nreqs;
        char *ldconfig;
        char *container_flags;

        /* list */
        bool compat32;
        bool list_bins;
        bool list_libs;
        bool list_ipcs;

        char *devices;
};

int select_devices(struct error *, char *, const struct nvc_device *[],
    const struct nvc_device [], size_t);

extern const struct argp info_usage;
extern const struct argp list_usage;
extern const struct argp configure_usage;

int info_command(const struct context *);
int list_command(const struct context *);
int configure_command(const struct context *);

#endif /* HEADER_CLI_H */
