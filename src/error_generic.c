/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "error_generic.h"
#include "utils.h"

void
error_reset(struct error *err)
{
        if (err == NULL)
                return;

        free(err->msg);
        err->msg = NULL;
        err->code = 0;
}

int
error_vset(struct error *err, int errcode, const char *errmsg, const char *fmt, va_list ap)
{
        char *msg = NULL;
        int rv = -1;

        if (err == NULL)
                return (0);

        error_reset(err);
        err->code = errcode;

        if (vasprintf(&msg, fmt, ap) < 0) {
                msg = NULL;
                goto fail;
        }
        if (errmsg == NULL) {
                err->msg = msg;
                return (0);
        }
        if (asprintf(&err->msg, "%s: %s", msg, errmsg) < 0) {
                err->msg = NULL;
                goto fail;
        }
        err->msg[strcspn(err->msg, "\n")] = '\0';
        str_lower(strrchr(err->msg, ':'));
        rv = 0;

 fail:
        free(msg);
        return (rv);
}

int
error_set(struct error *err, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = error_vset(err, errno, strerror(errno), fmt, ap);
        va_end(ap);
        return (rv);
}

int
error_setx(struct error *err, const char *fmt, ...)
{
        va_list ap;
        int rv;

        va_start(ap, fmt);
        rv = error_vset(err, -1, NULL, fmt, ap);
        va_end(ap);
        return (rv);
}
