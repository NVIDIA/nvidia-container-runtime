/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <inttypes.h>
#include <string.h>

#include "cli.h"

int
select_devices(struct error *err, char *devs, const struct nvc_device *selected[],
    const struct nvc_device available[], size_t size)
{
        char *gpu, *ptr;
        size_t i;
        uintmax_t n;

        while ((gpu = strsep(&devs, ",")) != NULL) {
                if (*gpu == '\0')
                        continue;
                if (!strcasecmp(gpu, "all")) {
                        for (i = 0; i < size; ++i)
                                selected[i] = &available[i];
                        break;
                }
                if (!strncasecmp(gpu, "GPU-", strlen("GPU-")) && strlen(gpu) > strlen("GPU-")) {
                        for (i = 0; i < size; ++i) {
                                if (!strncasecmp(available[i].uuid, gpu, strlen(gpu))) {
                                        selected[i] = &available[i];
                                        goto next;
                                }
                        }
                } else {
                        n = strtoumax(gpu, &ptr, 10);
                        if (*ptr == '\0' && n < UINTMAX_MAX && (size_t)n < size) {
                                selected[n] = &available[n];
                                goto next;
                        }
                }
                error_setx(err, "unknown device id: %s", gpu);
                return (-1);
         next: ;
        }
        return (0);
}
