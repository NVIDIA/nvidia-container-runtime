/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_ERROR_GENERIC_H
#define HEADER_ERROR_GENERIC_H

#include <stdarg.h>

struct error {
        int code;
        char *msg;
};

void error_reset(struct error *);
int error_vset(struct error *, int, const char *, const char *, va_list)
    __attribute__((format(printf, 4, 0), nonnull(4)));
int error_set(struct error *, const char *, ...)
    __attribute__((format(printf, 2, 3), nonnull(2)));
int error_setx(struct error *, const char *, ...)
    __attribute__((format(printf, 2, 3), nonnull(2)));

#endif /* HEADER_ERROR_GENERIC_H */
