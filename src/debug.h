/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_DEBUG_H
#define HEADER_DEBUG_H

#ifndef NDEBUG

#pragma GCC visibility push(default)
const char *__ubsan_default_options(void);
#pragma GCC visibility pop

const char *
__ubsan_default_options(void)
{
        return ("halt_on_error=1:print_stacktrace=1");
}

#endif /* NDEBUG */

#endif /* HEADER_DEBUG_H */
