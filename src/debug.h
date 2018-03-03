/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_DEBUG_H
#define HEADER_DEBUG_H

#ifndef NDEBUG

#include <sys/resource.h>

#pragma GCC visibility push(default)
const char *__ubsan_default_options(void);
#pragma GCC visibility pop

const char *
__ubsan_default_options(void)
{
        return ("halt_on_error=1:print_stacktrace=1");
}

static void __attribute__((constructor))
raise_ulimits(void)
{
    struct rlimit core = {RLIM_INFINITY, RLIM_INFINITY};

    assert(setrlimit(RLIMIT_CORE, &core) == 0);
}

#endif /* NDEBUG */

#endif /* HEADER_DEBUG_H */
