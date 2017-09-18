/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/types.h>

#include <alloca.h>
#include <argp.h>
#include <err.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "debug.h"
#include "dsl.h"
#include "error_generic.h"
#include "utils.h"

struct command;
struct context;

static bool is_root_dir(const char *);
static void print_version(FILE *, struct argp_state *);
static const struct command *parse_command(struct argp_state *);
static error_t main_parser(int, char *, struct argp_state *);
static error_t list_parser(int, char *, struct argp_state *);
static error_t configure_parser(int, char *, struct argp_state *);
static int list_command(const struct context *);
static int configure_command(const struct context *);
static int check_cuda_version(void *, enum dsl_comparator, const char *);
static int check_driver_version(void *, enum dsl_comparator, const char *);
static int select_gpu_devices(struct error *, char *, const struct nvc_device *[], const struct nvc_device [], size_t);

struct command {
        const char *name;
        const struct argp *argp;
        int (*func)(const struct context *);
};

struct context {
        uid_t uid;
        gid_t gid;
        pid_t pid;
        char *rootfs;
        char *devices;
        char *reqs[16];
        size_t nreqs;
        char *ldconfig;
        bool load_kmods;
        bool list_info;
        char *init_flags;
        char *driver_flags;
        char *device_flags;
        char *container_flags;

        const struct command *command;
};

#pragma GCC visibility push(default)
error_t argp_err_exit_status = EXIT_FAILURE;
void (*argp_program_version_hook)(FILE *, struct argp_state *) = &print_version;
const char *argp_program_bug_address = "https://github.com/NVIDIA/libnvidia-container/issues";
#pragma GCC visibility pop

static struct argp main_argp = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"debug", 'd', "FILE", 0, "Log debug information", -1},
                {"load-kmods", 'k', NULL, 0, "Load kernel modules", -1},
                {"userspec", 'u', "UID:GID", OPTION_ARG_OPTIONAL, "User and group to use for privilege separation", -1},
                {NULL, 0, NULL, 0, "Commands:", 0},
                {"list", 0, NULL, OPTION_DOC|OPTION_NO_USAGE, "List host driver components", 0},
                {"configure", 0, NULL, OPTION_DOC|OPTION_NO_USAGE, "Configure a container with GPU support", 0},
                {0},
        },
        main_parser,
        "COMMAND [ARG...]",
        "Command line utility for manipulating NVIDIA GPU containers.",
        NULL,
        NULL,
        NULL,
};

static const struct argp list_argp = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"info", 'i', NULL, 0, "List driver version information", -1},
                {"device", 'd', "ID", 0, "Device UUID(s) or index(es) to list", -1},
                {"compute", 'c', NULL, 0, "List compute components", -1},
                {"utility", 'u', NULL, 0, "List utility components", -1},
                {"video", 'v', NULL, 0, "List video components", -1},
                {"graphics", 'g', NULL, 0, "List graphics components", -1},
                {"compat32", 0x80, NULL, 0, "List 32bits components", -1},
                {0},
        },
        list_parser,
        NULL,
        "Query the host driver and list the components required in order to configure a container with GPU support.",
        NULL,
        NULL,
        NULL,
};

static const struct argp configure_argp = {
        (const struct argp_option[]){
                {NULL, 0, NULL, 0, "Options:", -1},
                {"pid", 'p', "PID", 0, "Container PID", -1},
                {"device", 'd', "ID", 0, "Device UUID(s) or index(es) to isolate", -1},
                {"require", 'r', "EXPR", 0, "Check container requirements", -1},
                {"ldconfig", 'l', "PATH", 0, "Path to the ldconfig binary", -1},
                {"compute", 'c', NULL, 0, "Enable compute capability", -1},
                {"utility", 'u', NULL, 0, "Enable utility capability", -1},
                {"video", 'v', NULL, 0, "Enable video capability", -1},
                {"graphics", 'g', NULL, 0, "Enable graphics capability", -1},
                {"compat32", 0x80, NULL, 0, "Enable 32bits compatibility", -1},
                {"no-cgroups", 0x81, NULL, 0, "Don't use cgroup enforcement", -1},
                {"no-devbind", 0x82, NULL, 0, "Don't bind mount devices", -1},
                {0},
        },
        configure_parser,
        "ROOTFS",
        "Configure a container with GPU support by exposing device drivers to it.\n\n"
        "This command enters the namespace of the container process referred by PID (or the current parent process if none specified) "
        "and performs the necessary steps to ensure that the given capabilities are available inside the container.\n"
        "It is assumed that the container has been created but not yet started, and the host filesystem is accessible (i.e. chroot/pivot_root hasn't been called).",
        NULL,
        NULL,
        NULL,
};

static const struct command commands[] = {
        {"list", &list_argp, &list_command},
        {"configure", &configure_argp, &configure_command},
};

static const struct dsl_rule rules[] = {
        {"cuda", &check_cuda_version},
        {"driver", &check_driver_version},
};

static bool
is_root_dir(const char *path)
{
        bool rv = false;
        char *p;

        p = realpath(path, NULL);
        if (p != NULL && !strcmp(p, "/"))
                rv = true;
        free(p);
        return (rv);
}

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
parse_command(struct argp_state *state)
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
main_parser(int key, char *arg, struct argp_state *state)
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
                ctx->command = parse_command(state);
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

static error_t
list_parser(int key, char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;
        struct error err = {0};

        switch (key) {
        case 'i':
                ctx->list_info = true;
                break;
        case 'd':
                if (strjoin(&err, &ctx->devices, arg, ",") < 0)
                        goto fatal;
                break;
        case 'c':
                if (strjoin(&err, &ctx->driver_flags, "compute", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "compute", " ") < 0)
                        goto fatal;
                break;
        case 'u':
                if (strjoin(&err, &ctx->driver_flags, "utility", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "utility", " ") < 0)
                        goto fatal;
                break;
        case 'v':
                if (strjoin(&err, &ctx->driver_flags, "video", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "video", " ") < 0)
                        goto fatal;
                break;
        case 'g':
                if (strjoin(&err, &ctx->driver_flags, "graphics", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "graphics", " ") < 0)
                        goto fatal;
                break;
        case 0x80:
                if (strjoin(&err, &ctx->driver_flags, "compat32", " ") < 0)
                        goto fatal;
                break;
        default:
                return (ARGP_ERR_UNKNOWN);
        }
        return (0);

 fatal:
        errx(EXIT_FAILURE, "input error: %s", err.msg);
        return (0);
}

static error_t
configure_parser(int key, char *arg, struct argp_state *state)
{
        struct context *ctx = state->input;
        struct error err = {0};

        switch (key) {
        case 'p':
                if (strtopid(&err, arg, &ctx->pid) < 0)
                        goto fatal;
                break;
        case 'd':
                if (strjoin(&err, &ctx->devices, arg, ",") < 0)
                        goto fatal;
                break;
        case 'r':
                if (ctx->nreqs >= nitems(ctx->reqs)) {
                        error_setx(&err, "too many requirements");
                        goto fatal;
                }
                ctx->reqs[ctx->nreqs++] = arg;
                break;
        case 'l':
                ctx->ldconfig = arg;
                break;
        case 'c':
                if (strjoin(&err, &ctx->driver_flags, "compute", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "compute", " ") < 0)
                        goto fatal;
                break;
        case 'u':
                if (strjoin(&err, &ctx->driver_flags, "utility", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "utility", " ") < 0)
                        goto fatal;
                break;
        case 'v':
                if (strjoin(&err, &ctx->driver_flags, "video", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "video", " ") < 0)
                        goto fatal;
                break;
        case 'g':
                if (strjoin(&err, &ctx->driver_flags, "graphics", " ") < 0 ||
                    strjoin(&err, &ctx->device_flags, "graphics", " ") < 0)
                        goto fatal;
                break;
        case 0x80:
                if (strjoin(&err, &ctx->driver_flags, "compat32", " ") < 0)
                        goto fatal;
                break;
        case 0x81:
                if (strjoin(&err, &ctx->container_flags, "no-cgroups", " ") < 0)
                        goto fatal;
                break;
        case 0x82:
                if (strjoin(&err, &ctx->container_flags, "no-devbind", " ") < 0)
                        goto fatal;
                break;
        case ARGP_KEY_ARG:
                if (state->arg_num > 0)
                        argp_usage(state);
                if (is_root_dir(arg)) {
                        error_setx(&err, "invalid rootfs directory");
                        goto fatal;
                }
                ctx->rootfs = arg;
                break;
        case ARGP_KEY_SUCCESS:
                if (ctx->pid > 0) {
                        if (strjoin(&err, &ctx->container_flags, "supervised", " ") < 0)
                                goto fatal;
                } else {
                        ctx->pid = getppid();
                        if (strjoin(&err, &ctx->container_flags, "standalone", " ") < 0)
                                goto fatal;
                }
                break;
        case ARGP_KEY_END:
                if (state->arg_num < 1)
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

static int
check_cuda_version(void *data, enum dsl_comparator cmp, const char *version)
{
        const struct nvc_driver_info *drv = data;
        return (dsl_compare_version(drv->cuda_version, cmp, version));
}

static int
check_driver_version(void *data, enum dsl_comparator cmp, const char *version)
{
        const struct nvc_driver_info *drv = data;
        return (dsl_compare_version(drv->nvrm_version, cmp, version));
}

static int
select_gpu_devices(struct error *err, char *devs, const struct nvc_device *selected[],
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

static int
list_command(const struct context *ctx)
{
        bool run_as_root;
        struct nvc_context *nvc = NULL;
        struct nvc_config *nvc_cfg = NULL;
        struct nvc_driver_info *drv = NULL;
        struct nvc_device_info *dev = NULL;
        const struct nvc_device **gpus = NULL;
        struct error err = {0};
        int rv = EXIT_FAILURE;

        run_as_root = (geteuid() == 0);
        if (!run_as_root && ctx->load_kmods) {
                warnx("requires root privileges");
                return (rv);
        }
        if (run_as_root) {
                if (perm_set_capabilities(&err, CAP_PERMITTED, permitted_caps, nitems(permitted_caps)) < 0 ||
                    perm_set_capabilities(&err, CAP_INHERITABLE, inherited_caps, nitems(inherited_caps)) < 0 ||
                    perm_drop_bounds(&err) < 0) {
                        warnx("permission error: %s", err.msg);
                        return (rv);
                }
        } else {
                if (perm_set_capabilities(&err, CAP_PERMITTED, NULL, 0) < 0) {
                        warnx("permission error: %s", err.msg);
                        return (rv);
                }
        }

        /* Initialize the library context. */
        int c = ctx->load_kmods ? CAPS_INIT_KMODS : CAPS_INIT;
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[c], effective_caps_size(c)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((nvc = nvc_context_new()) == NULL ||
            (nvc_cfg = nvc_config_new()) == NULL) {
                warn("memory allocation failed");
                goto fail;
        }
        nvc_cfg->uid = (!run_as_root && ctx->uid == (uid_t)-1) ? geteuid() : ctx->uid;
        nvc_cfg->gid = (!run_as_root && ctx->gid == (gid_t)-1) ? getegid() : ctx->gid;
        if (nvc_init(nvc, nvc_cfg, ctx->init_flags) < 0) {
                warnx("initialization error: %s", nvc_error(nvc));
                goto fail;
        }

        /* Query the driver and device information. */
        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_INFO], effective_caps_size(CAPS_INFO)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((drv = nvc_driver_info_new(nvc, (ctx->driver_flags != NULL) ? ctx->driver_flags : "")) == NULL ||
            (dev = nvc_device_info_new(nvc, (ctx->device_flags != NULL) ? ctx->device_flags : "")) == NULL) {
                warnx("detection error: %s", nvc_error(nvc));
                goto fail;
        }

        /* List the driver information. */
        if (ctx->list_info) {
                printf("NVRM version: %s\n", drv->nvrm_version);
                printf("CUDA version: %s\n", drv->cuda_version);
        }

        /* List the visible GPU devices. */
        if (dev->ngpus > 0) {
                gpus = alloca(dev->ngpus * sizeof(*gpus));
                memset(gpus, 0, dev->ngpus * sizeof(*gpus));
                if (select_gpu_devices(&err, ctx->devices, gpus, dev->gpus, dev->ngpus) < 0) {
                        warnx("device error: %s", err.msg);
                        goto fail;
                }
        }
        if (ctx->devices != NULL) {
                for (size_t i = 0; i < drv->ndevs; ++i)
                        printf("%s\n", drv->devs[i].path);
                for (size_t i = 0; i < dev->ngpus; ++i) {
                        if (gpus[i] != NULL)
                                printf("%s\n", gpus[i]->node.path);
                }
        }

        /* List the driver components */
        for (size_t i = 0; i < drv->nbins; ++i)
                printf("%s\n", drv->bins[i]);
        for (size_t i = 0; i < drv->nlibs; ++i)
                printf("%s\n", drv->libs[i]);
        for (size_t i = 0; i < drv->nlibs32; ++i)
                printf("%s\n", drv->libs32[i]);
        for (size_t i = 0; i < drv->nipcs; ++i)
                printf("%s\n", drv->ipcs[i]);

        if (run_as_root && perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_SHUTDOWN], effective_caps_size(CAPS_SHUTDOWN)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        rv = EXIT_SUCCESS;
 fail:
        nvc_shutdown(nvc);
        nvc_device_info_free(dev);
        nvc_driver_info_free(drv);
        nvc_config_free(nvc_cfg);
        nvc_context_free(nvc);
        error_reset(&err);
        return (rv);
}

static int
configure_command(const struct context *ctx)
{
        struct nvc_context *nvc = NULL;
        struct nvc_config *nvc_cfg = NULL;
        struct nvc_driver_info *drv = NULL;
        struct nvc_device_info *dev = NULL;
        struct nvc_container *cnt = NULL;
        struct nvc_container_config *cnt_cfg = NULL;
        const struct nvc_device **gpus = NULL;
        struct error err = {0};
        int rv = EXIT_FAILURE;

        if (geteuid() != 0) {
                warnx("requires root privileges");
                return (rv);
        }
        if (perm_set_capabilities(&err, CAP_PERMITTED, permitted_caps, nitems(permitted_caps)) < 0 ||
            perm_set_capabilities(&err, CAP_INHERITABLE, inherited_caps, nitems(inherited_caps)) < 0 ||
            perm_drop_bounds(&err) < 0) {
                warnx("permission error: %s", err.msg);
                return (rv);
        }

        /* Initialize the library and container contexts. */
        int c = ctx->load_kmods ? CAPS_INIT_KMODS : CAPS_INIT;
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[c], effective_caps_size(c)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((nvc = nvc_context_new()) == NULL ||
            (nvc_cfg = nvc_config_new()) == NULL ||
            (cnt_cfg = nvc_container_config_new(ctx->pid, ctx->rootfs)) == NULL) {
                warn("memory allocation failed");
                goto fail;
        }
        nvc_cfg->uid = ctx->uid;
        nvc_cfg->gid = ctx->gid;
        if (nvc_init(nvc, nvc_cfg, ctx->init_flags) < 0) {
                warnx("initialization error: %s", nvc_error(nvc));
                goto fail;
        }
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_CONTAINER], effective_caps_size(CAPS_CONTAINER)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        cnt_cfg->ldconfig = ctx->ldconfig;
        if ((cnt = nvc_container_new(nvc, cnt_cfg, ctx->container_flags)) == NULL) {
                warnx("container error: %s", nvc_error(nvc));
                goto fail;
        }

        /* Query the driver and device information. */
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_INFO], effective_caps_size(CAPS_INFO)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if ((drv = nvc_driver_info_new(nvc, ctx->driver_flags)) == NULL ||
            (dev = nvc_device_info_new(nvc, ctx->device_flags)) == NULL) {
                warnx("detection error: %s", nvc_error(nvc));
                goto fail;
        }

        /* Check the container requirements. */
        for (size_t i = 0; i < ctx->nreqs; ++i) {
                if (dsl_evaluate(&err, ctx->reqs[i], drv, rules, nitems(rules)) < 0) {
                        warnx("requirement error: %s", err.msg);
                        goto fail;
                }
        }

        /* Select the visible GPU devices. */
        if (dev->ngpus > 0) {
                gpus = alloca(dev->ngpus * sizeof(*gpus));
                memset(gpus, 0, dev->ngpus * sizeof(*gpus));
                if (select_gpu_devices(&err, ctx->devices, gpus, dev->gpus, dev->ngpus) < 0) {
                        warnx("device error: %s", err.msg);
                        goto fail;
                }
        }

        /* Mount the driver and visible devices. */
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_MOUNT], effective_caps_size(CAPS_MOUNT)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if (nvc_driver_mount(nvc, cnt, drv) < 0) {
                warnx("mount error: %s", nvc_error(nvc));
                goto fail;
        }
        for (size_t i = 0; i < dev->ngpus; ++i) {
                if (gpus[i] != NULL && nvc_device_mount(nvc, cnt, gpus[i]) < 0) {
                        warnx("mount error: %s", nvc_error(nvc));
                        goto fail;
                }
        }

        /* Update the container ldcache. */
        if (perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_LDCACHE], effective_caps_size(CAPS_LDCACHE)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        if (nvc_ldcache_update(nvc, cnt) < 0) {
                warnx("ldcache error: %s", nvc_error(nvc));
                goto fail;
        }

        if (perm_set_capabilities(&err, CAP_EFFECTIVE, effective_caps[CAPS_SHUTDOWN], effective_caps_size(CAPS_SHUTDOWN)) < 0) {
                warnx("permission error: %s", err.msg);
                goto fail;
        }
        rv = EXIT_SUCCESS;
 fail:
        nvc_shutdown(nvc);
        nvc_container_free(cnt);
        nvc_device_info_free(dev);
        nvc_driver_info_free(drv);
        nvc_container_config_free(cnt_cfg);
        nvc_config_free(nvc_cfg);
        nvc_context_free(nvc);
        error_reset(&err);
        return (rv);
}

int
main(int argc, char *argv[])
{
        struct context ctx = {.uid = (uid_t)-1, .gid = (gid_t)-1};
        int rv;

        argp_parse(&main_argp, argc, argv, ARGP_IN_ORDER, NULL, &ctx);
        rv = ctx.command->func(&ctx);

        free(ctx.devices);
        free(ctx.init_flags);
        free(ctx.driver_flags);
        free(ctx.device_flags);
        free(ctx.container_flags);
        return (rv);
}
