/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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
        uid_t uid;
        gid_t gid;
        pid_t pid;
        char *rootfs;
        char *devices;
        char *reqs[32];
        size_t nreqs;
        char *ldconfig;
        bool load_kmods;
        bool list_info;
        char *init_flags;
        char *driver_flags;
        char *device_flags;
        char *container_flags;

        const struct command *command;
};

int select_devices(struct error *, char *, const struct nvc_device *[],
    const struct nvc_device [], size_t);

extern const struct argp list_usage;
extern const struct argp configure_usage;

int list_command(const struct context *);
int configure_command(const struct context *);

#endif /* HEADER_CLI_H */
