/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <stdlib.h>
#include <string.h>

#include "nvc_internal.h"

#include "error.h"
#include "options.h"
#include "xfuncs.h"

int32_t
options_parse(struct error *err, const char *str, const struct option *opts, size_t nopts)
{
        char buf[NVC_ARG_MAX];
        char *ptr = buf;
        int32_t flags = 0;
        char *opt;
        size_t i;

        if (strlen(str) >= sizeof(buf)) {
                error_setx(err, "too many options");
                return (-1);
        }
        strcpy(buf, str);

        while ((opt = strsep(&ptr, " ")) != NULL) {
                if (*opt == '\0')
                        continue;
                for (i = 0; i < nopts; ++i) {
                        if (str_equal(opt, opts[i].name)) {
                                flags |= opts[i].value;
                                break;
                        }
                }
                if (i == nopts) {
                        error_setx(err, "invalid option: %s", opt);
                        return (-1);
                }
        }
        return (flags);
}
