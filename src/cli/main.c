/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/types.h>

#include <alloca.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "debug.h"
#include "utils.h"

static void print_version(FILE *, struct argp_state *);
static const struct command *lookup_command(struct argp_state *);
static error_t parser(int, char *, struct argp_state *);

#pragma GCC visibility push(default)
error_t argp_err_exit_status = EXIT_FAILURE;
void (*argp_program_version_hook)(FILE *, struct argp_state *) = &print_version;
const char *argp_program_bug_address = "https://github.com/NVIDIA/libnvidia-container/issues";
#pragma GCC visibility pop

static struct argp usage = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"debug", 'd', "FILE", 0, "Log debug information", -1},
                {"load-kmods", 'k', NULL, 0, "Load kernel modules", -1},
                {"user", 'u', "UID[:GID]", OPTION_ARG_OPTIONAL, "User and group to use for privilege separation", -1},
                {NULL, 0, NULL, 0, "Commands:", 0},
                {"info", 0, NULL, OPTION_DOC|OPTION_NO_USAGE, "Report information about the driver and devices", 0},
                {"list", 0, NULL, OPTION_DOC|OPTION_NO_USAGE, "List driver components", 0},
                {"configure", 0, NULL, OPTION_DOC|OPTION_NO_USAGE, "Configure a container with GPU support", 0},
                {0},
        },
        parser,
        "COMMAND [ARG...]",
        "Command line utility for manipulating NVIDIA GPU containers.",
        NULL,
        NULL,
        NULL,
};

static const struct command commands[] = {
        {"info", &info_usage, &info_command},
        {"list", &list_usage, &list_command},
        {"configure", &configure_usage, &configure_command},
};

static void
print_version(FILE *stream, maybe_unused struct argp_state *state)
{
        fprintf(stream, "version: %s\n", NVC_VERSION);
        fprintf(stream, "build date: %s\n", BUILD_DATE);
        fprintf(stream, "build revision: %s\n", BUILD_REVISION);
        fprintf(stream, "build compiler: %s\n", BUILD_COMPILER);
        fprintf(stream, "build flags: %s\n", BUILD_FLAGS);
}

static const struct command *
lookup_command(struct argp_state *state)
{
        for (size_t i = 0; i < nitems(commands); ++i) {
                if (!strcmp(state->argv[0], commands[i].name)) {
                        state->argv[0] = alloca(strlen(state->name) + strlen(commands[i].name) + 2);
                        sprintf(state->argv[0], "%s %s", state->name, commands[i].name);
                        argp_parse(commands[i].argp, state->argc, state->argv, 0, NULL, state->input);
                        return (&commands[i]);
                }
        }
        argp_usage(state);
        return (NULL);
}

static error_t
parser(int key, char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;
        struct error err = {0};

        switch (key) {
        case 'd':
                setenv("NVC_DEBUG_FILE", arg, 1);
                break;
        case 'k':
                ctx->load_kmods = true;
                if (strjoin(&err, &ctx->init_flags, "load-kmods", " ") < 0)
                        goto fatal;
                break;
        case 'u':
                if (arg != NULL) {
                        if (strtougid(&err, arg, &ctx->uid, &ctx->gid) < 0)
                                goto fatal;
                } else {
                        ctx->uid = geteuid();
                        ctx->gid = getegid();
                }
                break;
        case ARGP_KEY_ARGS:
                state->argv += state->next;
                state->argc -= state->next;
                ctx->command = lookup_command(state);
                break;
        case ARGP_KEY_END:
                if (ctx->command == NULL)
                        argp_usage(state);
                break;
        default:
                return (ARGP_ERR_UNKNOWN);
        }
        return (0);

 fatal:
        errx(EXIT_FAILURE, "input error: %s", err.msg);
        return (0);
}

int
main(int argc, char *argv[])
{
        struct context ctx = {.uid = (uid_t)-1, .gid = (gid_t)-1};
        int rv;

        argp_parse(&usage, argc, argv, ARGP_IN_ORDER, NULL, &ctx);
        rv = ctx.command->func(&ctx);

        free(ctx.devices);
        free(ctx.init_flags);
        free(ctx.container_flags);
        return (rv);
}
