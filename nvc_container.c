/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/types.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

typedef char *(*parse_fn)(char *, const char *);

static char *cgroup_mount(char *, const char *);
static char *cgroup_root(char *, const char *);
static char *parse_proc_file(struct error *, const char *, parse_fn, const char *);
static char *find_cgroup_path(struct error *, const struct nvc_container *, const char *);
static char *find_namespace_path(struct error *, const struct nvc_container *, const char *);
static int  lookup_owner(struct error *, struct nvc_container *);
static int  copy_config(struct error *, struct nvc_container *, const struct nvc_container_config *);

struct nvc_container_config *
nvc_container_config_new(pid_t pid, const char *rootfs)
{
        struct nvc_container_config *cfg;

        if ((cfg = calloc(1, sizeof(*cfg))) == NULL)
                return (NULL);

        cfg->pid = pid;
        cfg->rootfs = (char *)rootfs;
        return (cfg);
}

void
nvc_container_config_free(struct nvc_container_config *cfg)
{
        if (cfg == NULL)
                return;
        free(cfg);
}

static char *
cgroup_mount(char *line, const char *subsys)
{
        char *root, *mount, *fstype, *substr;

        for (int i = 0; i < 4; ++i)
                root = strsep(&line, " ");
        mount = strsep(&line, " ");
        line = strchr(line, '-');
        for (int i = 0; i < 2; ++i)
                fstype = strsep(&line, " ");
        for (int i = 0; i < 2; ++i)
                substr = strsep(&line, " ");

        if (root == NULL || mount == NULL || fstype == NULL || substr == NULL)
                return (NULL);
        if (strcmp(root, "/"))
                return (NULL);
        if (strcmp(fstype, "cgroup"))
                return (NULL);
        if (strstr(substr, subsys) == NULL)
                return (NULL);

        return (mount);
}

static char *
cgroup_root(char *line, const char *subsys)
{
        char *root, *substr, *ptr;

        for (int i = 0; i < 2; ++i)
                substr = strsep(&line, ":");
        root = strsep(&line, ":");

        if (root == NULL || substr == NULL)
                return (NULL);
        if (!strpcmp(root, "/.."))
                return (NULL);
        if (strstr(substr, subsys) == NULL)
                return (NULL);

        if ((ptr = strchr(root, '\n')) != NULL)
                *ptr = '\0';
        return (root);
}

static char *
parse_proc_file(struct error *err, const char *procf, parse_fn parse, const char *subsys)
{
        FILE *fs;
        ssize_t n;
        char *buf = NULL;
        size_t len = 0;
        char *ptr = NULL;
        char *path = NULL;

        if ((fs = xfopen(err, procf, "r")) == NULL)
                return (NULL);
        while ((n = getline(&buf, &len, fs)) >= 0) {
                ptr = buf;
                if ((ptr = parse(ptr, subsys)) != NULL)
                        break;
        }
        if (ferror(fs)) {
                error_set(err, "read error: %s", procf);
                goto fail;
        }
        if (ptr == NULL || feof(fs)) {
                error_setx(err, "cgroup subsystem %s not found", subsys);
                goto fail;
        }
        path = xstrdup(err, ptr);

 fail:
        free(buf);
        fclose(fs);
        return (path);
}

static char *
find_cgroup_path(struct error *err, const struct nvc_container *cnt, const char *subsys)
{
        pid_t pid;
        const char *prefix;
        char path[PATH_MAX];
        char *mount = NULL;
        char *root = NULL;
        char *cgroup = NULL;

        pid = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.pid : getpid();
        prefix = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "";

        if (xsnprintf(err, path, sizeof(path), "%s"PROC_MOUNTS_PATH(PROC_PID), prefix, (long)pid) < 0)
                goto fail;
        if ((mount = parse_proc_file(err, path, cgroup_mount, subsys)) == NULL)
                goto fail;
        if (xsnprintf(err, path, sizeof(path), "%s"PROC_CGROUP_PATH(PROC_PID), prefix, (long)cnt->cfg.pid) < 0)
                goto fail;
        if ((root = parse_proc_file(err, path, cgroup_root, subsys)) == NULL)
                goto fail;
        xasprintf(err, &cgroup, "%s%s%s", prefix, mount, root);

 fail:
        free(mount);
        free(root);
        return (cgroup);
}

static char *
find_namespace_path(struct error *err, const struct nvc_container *cnt, const char *namespace)
{
        const char *prefix;
        char *ns = NULL;

        prefix = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "";
        xasprintf(err, &ns, "%s"PROC_NS_PATH(PROC_PID), prefix, (long)cnt->cfg.pid, namespace);
        return (ns);
}

static int
lookup_owner(struct error *err, struct nvc_container *cnt)
{
        const char *prefix;
        char path[PATH_MAX];
        struct stat s;

        prefix = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "";
        if (xsnprintf(err, path, sizeof(path), "%s"PROC_PID, prefix, (long)cnt->cfg.pid) < 0)
                return (-1);
        if (xstat(err, path, &s) < 0)
                return (-1);
        cnt->uid = s.st_uid;
        cnt->gid = s.st_gid;
        return (0);
}

static int
copy_config(struct error *err, struct nvc_container *cnt, const struct nvc_container_config *cfg)
{
        char path[PATH_MAX];
        char tmp[PATH_MAX];
        const char *bins_dir = cfg->bins_dir;
        const char *libs_dir = cfg->libs_dir;
        const char *libs32_dir = cfg->libs32_dir;
        const char *ldconfig = cfg->ldconfig;
        int multiarch, ret;

        if (bins_dir == NULL)
                bins_dir = USR_BIN_DIR;
        if (libs_dir == NULL || libs32_dir == NULL) {
                /* Debian and its derivatives use a multiarch directory scheme. */
                if (path_resolve(err, path, cfg->rootfs, "/etc/debian_version") < 0)
                        return (-1);
                if ((multiarch = file_exists(err, path)) < 0)
                        return (-1);

                if (multiarch) {
                        if (libs_dir == NULL)
                                libs_dir = USR_LIB_MULTIARCH_DIR;
                        if (libs32_dir == NULL)
                                libs32_dir = USR_LIB32_MULTIARCH_DIR;
                } else {
                        if (libs_dir == NULL)
                                libs_dir = USR_LIB_DIR;
                        if (libs32_dir == NULL) {
                                /*
                                 * The lib32 directory is inconsistent accross distributions.
                                 * Check which one is used in the rootfs.
                                 */
                                libs32_dir = USR_LIB32_DIR;
                                if (path_resolve(err, path, cfg->rootfs, USR_LIB32_DIR) < 0)
                                        return (-1);
                                if ((ret = file_exists(err, path)) < 0)
                                        return (-1);
                                if (!ret) {
                                        if (path_resolve(err, tmp, cfg->rootfs, libs_dir) < 0)
                                                return (-1);
                                        if (path_resolve(err, path, cfg->rootfs, USR_LIB32_ALT_DIR) < 0)
                                                return (-1);
                                        if ((ret = file_exists(err, path)) < 0)
                                                return (-1);
                                        if (ret && strcmp(path, tmp))
                                                libs32_dir = USR_LIB32_ALT_DIR;
                                }
                        }
                }
        }
        if (ldconfig == NULL) {
                /*
                 * Some distributions have a wrapper script around ldconfig to reduce package install time.
                 * Always refer to the real one to prevent having our privileges dropped by a shebang.
                 */
                if (path_resolve(err, path, cfg->rootfs, LDCONFIG_ALT_PATH) < 0)
                        return (-1);
                if ((ret = file_exists(err, path)) < 0)
                        return (-1);
                ldconfig = ret ? LDCONFIG_ALT_PATH : LDCONFIG_PATH;
        }

        cnt->cfg.pid = cfg->pid;
        if ((cnt->cfg.rootfs = xrealpath(err, cfg->rootfs, NULL)) == NULL)
                return (-1);
        if ((cnt->cfg.bins_dir = xstrdup(err, bins_dir)) == NULL)
                return (-1);
        if ((cnt->cfg.libs_dir = xstrdup(err, libs_dir)) == NULL)
                return (-1);
        if ((cnt->cfg.libs32_dir = xstrdup(err, libs32_dir)) == NULL)
                return (-1);
        if ((cnt->cfg.ldconfig = xstrdup(err, ldconfig)) == NULL)
                return (-1);

        return (0);
}

struct nvc_container *
nvc_container_new(struct nvc_context *ctx, const struct nvc_container_config *cfg, const char *opts)
{
        struct nvc_container *cnt;
        int32_t flags;

        if (validate_context(ctx) < 0)
                return (NULL);
        if (validate_args(ctx, cfg != NULL && cfg->pid > 0 && cfg->rootfs != NULL) < 0)
                return (NULL);
        if (opts == NULL)
                opts = default_container_opts;
        if ((flags = options_parse(&ctx->err, opts, container_opts, nitems(container_opts))) < 0)
                return (NULL);
        if ((!(flags & OPT_SUPERVISED) ^ !(flags & OPT_STANDALONE)) == 0) {
                error_setx(&ctx->err, "invalid mode of operation");
                return (NULL);
        }

        log_info("configuring container with '%s'", opts);
        if ((cnt = xcalloc(&ctx->err, 1, sizeof(*cnt))) == NULL)
                return (NULL);

        cnt->flags = flags;
        if (copy_config(&ctx->err, cnt, cfg) < 0)
                goto fail;
        if (lookup_owner(&ctx->err, cnt) < 0)
                goto fail;
        if ((cnt->mnt_ns = find_namespace_path(&ctx->err, cnt, "mnt")) == NULL)
                goto fail;
        if (!(flags & OPT_NO_CGROUPS)) {
                if ((cnt->dev_cg = find_cgroup_path(&ctx->err, cnt, "devices")) == NULL)
                        goto fail;
        }

        log_info("setting pid to %ld", (long)cnt->cfg.pid);
        log_info("setting rootfs to %s", cnt->cfg.rootfs);
        log_info("setting owner to %lu:%lu", (unsigned long)cnt->uid, (unsigned long)cnt->gid);
        log_info("setting bins directory to %s", cnt->cfg.bins_dir);
        log_info("setting libs directory to %s", cnt->cfg.libs_dir);
        log_info("setting libs32 directory to %s", cnt->cfg.libs32_dir);
        log_info("setting ldconfig to %s", cnt->cfg.ldconfig);
        log_info("setting mount namespace to %s", cnt->mnt_ns);
        if (!(flags & OPT_NO_CGROUPS))
                log_info("setting devices cgroup to %s", cnt->dev_cg);
        return (cnt);

 fail:
        nvc_container_free(cnt);
        return (NULL);
}

void
nvc_container_free(struct nvc_container *cnt)
{
        if (cnt == NULL)
                return;
        free(cnt->cfg.rootfs);
        free(cnt->cfg.bins_dir);
        free(cnt->cfg.libs_dir);
        free(cnt->cfg.libs32_dir);
        free(cnt->cfg.ldconfig);
        free(cnt->mnt_ns);
        free(cnt->dev_cg);
        free(cnt);
}
