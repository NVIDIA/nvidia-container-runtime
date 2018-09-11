/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_DSL_H
#define HEADER_DSL_H

#include <stddef.h>

#include "cli.h"

enum dsl_comparator {
        EQUAL,
        NOT_EQUAL,
        LESS,
        LESS_EQUAL,
        GREATER,
        GREATER_EQUAL,
};

struct dsl_data {
        struct nvc_driver_info *drv;
        const struct nvc_device *dev;
};

struct dsl_rule {
        const char *name;
        int (*func)(const struct dsl_data *, enum dsl_comparator, const char *);
};

int dsl_compare_version(const char *, enum dsl_comparator, const char *);
int dsl_compare_string(const char *, enum dsl_comparator, const char *);
int dsl_evaluate(struct error *, const char *, void *, const struct dsl_rule [], size_t);

#endif /* HEADER_DSL_H */
