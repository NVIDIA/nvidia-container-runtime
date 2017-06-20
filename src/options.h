/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_OPTIONS_H
#define HEADER_OPTIONS_H

#include <stdint.h>

#include "error.h"

struct option {
        const char *name;
        int32_t value;
};

/* Library options */
enum {
        OPT_LOAD_KMODS = 1 << 0,
};

static const struct option library_opts[] = {
        {"load-kmods", OPT_LOAD_KMODS},
};

static const char * const default_library_opts = "";

/* Driver options */
enum {
        OPT_UTILITY_LIBS    = 1 << 0,
        OPT_COMPUTE_LIBS    = 1 << 1,
        OPT_VIDEO_LIBS      = 1 << 2,
        OPT_GRAPHIC_LIBS    = 1 << 3,
        OPT_UTILITY_BINS    = 1 << 4,
        OPT_COMPUTE_BINS    = 1 << 5,
        OPT_COMPAT32        = 1 << 6,
        OPT_NO_GLVND        = 1 << 7,
        OPT_NO_UVM          = 1 << 8,
        OPT_NO_MPS          = 1 << 9,
        OPT_NO_PERSISTENCED = 1 << 10,
};

static const struct option driver_opts[] = {
        {"utility", OPT_UTILITY_BINS|OPT_UTILITY_LIBS},
        {"compute", OPT_COMPUTE_BINS|OPT_COMPUTE_LIBS},
        {"video", OPT_VIDEO_LIBS},
        {"graphic", OPT_GRAPHIC_LIBS},
        {"compat32", OPT_COMPAT32},
        {"no-glvnd", OPT_NO_GLVND},
        {"no-uvm", OPT_NO_UVM},
        {"no-mps", OPT_NO_MPS},
        {"no-persistenced", OPT_NO_PERSISTENCED},
};

static const char * const default_driver_opts = "utility compute video graphic compat32";

/* Device options */
static const struct option device_opts[] = {
        {"utility", 0},
        {"compute", 0},
        {"video", 0},
        {"graphic", 0},
};

static const char * const default_device_opts = "utility compute video graphic";

/* Container options */
enum {
        OPT_SUPERVISED = 1 << 0,
        OPT_STANDALONE = 1 << 1,
        OPT_NO_CGROUPS = 1 << 2,
        OPT_NO_DEVBIND = 1 << 3,
};

static const struct option container_opts[] = {
        {"supervised", OPT_SUPERVISED},
        {"standalone", OPT_STANDALONE},
        {"no-cgroups", OPT_NO_CGROUPS},
        {"no-devbind", OPT_NO_DEVBIND},
};

static const char * const default_container_opts = "standalone no-cgroups no-devbind";

int32_t options_parse(struct error *, const char *, const struct option *, size_t);

#endif /* HEADER_OPTIONS_H */
