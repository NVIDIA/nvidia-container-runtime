/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_DRIVER_H
#define HEADER_DRIVER_H

#include <rpc/rpc.h>

#ifndef WITH_TIRPC
/* Glibc is missing this prototype */
SVCXPRT *svcunixfd_create(int, u_int, u_int);
#endif /* WITH_TIRPC */

#include <stdbool.h>

#include "error.h"

#define SOCK_CLT 0
#define SOCK_SVC 1

struct driver {
        struct error *err;
        void *cuda_dl;
        void *nvml_dl;
        int fd[2];
        pid_t pid;
        SVCXPRT *rpc_svc;
        CLIENT *rpc_clt;
};

typedef struct nvmlDevice_st *driver_device_handle;

void driver_program_1(struct svc_req *, register SVCXPRT *);

int driver_init(struct driver *, struct error *);
int driver_shutdown(struct driver *);
int driver_get_rm_version(struct driver *, char **);
int driver_get_cuda_version(struct driver *, char **);
int driver_get_device_count(struct driver *, unsigned int *);
int driver_get_device_handle(struct driver *, unsigned int, driver_device_handle *, bool);
int driver_get_device_minor(struct driver *, driver_device_handle, unsigned int *);
int driver_get_device_busid(struct driver *, driver_device_handle, char **);
int driver_get_device_uuid(struct driver *, driver_device_handle, char **);

#endif /* HEADER_DRIVER_H */
