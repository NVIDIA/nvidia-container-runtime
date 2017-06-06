/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <gnu/lib-names.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nvc_internal.h"

const char nvc_interp[] __attribute__((section(".interp"))) = LIB_DIR "/" LD_SO;

void
nvc_entry(void)
{
        printf("version: %s\n", NVC_VERSION);
        printf("build date: %s\n", BUILD_DATE);
        printf("build revision: %s\n", BUILD_REVISION);
        printf("build flags: %s\n", BUILD_FLAGS);
        exit(EXIT_SUCCESS);
}
