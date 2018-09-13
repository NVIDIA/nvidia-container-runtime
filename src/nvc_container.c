/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/types.h>

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "common.h"
#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

typedef char *(*parse_fn)(char *, char *, const char *);

static char *cgroup_mount(char *, char *, const char *);
static char *cgroup_root(char *, char *, const char *);
static char *parse_proc_file(struct error *, const char *, parse_fn, char *, const char *);
static char *find_cgroup_path(struct error *, const struct nvc_container *, const char *);
static char *find_namespace_path(struct error *, const struct nvc_container *, const char *);
static int  find_library_paths(struct error *, struct nvc_container *);
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
cgroup_mount(char *line, char *prefix, const char *subsys)
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
        if (*root == '\0' || *mount == '\0' || *fstype == '\0' || *substr == '\0')
                return (NULL);
        if (!str_equal(fstype, "cgroup"))
                return (NULL);
        if (strstr(substr, subsys) == NULL)
                return (NULL);
        if (strlen(root) >= PATH_MAX || str_has_prefix(root, "/.."))
                return (NULL);
        strcpy(prefix, root);

        return (mount);
}

static char *
cgroup_root(char *line, char *prefix, const char *subsys)
{
        char *root, *substr;

        for (int i = 0; i < 2; ++i)
                substr = strsep(&line, ":");
        root = strsep(&line, ":");

        if (root == NULL || substr == NULL)
                return (NULL);
        if (*root == '\0' || *substr == '\0')
                return (NULL);
        if (strstr(substr, subsys) == NULL)
                return (NULL);
        if (strlen(root) >= PATH_MAX || str_has_prefix(root, "/.."))
                return (NULL);
        if (!str_equal(prefix, "/") && str_has_prefix(root, prefix))
                root += strlen(prefix);

        return (root);
}

static char *
parse_proc_file(struct error *err, const char *procf, parse_fn parse, char *prefix, const char *subsys)
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
                ptr[strcspn(ptr, "\n")] = '\0';
                if (n == 0 || *ptr == '\0')
                        continue;
                if ((ptr = parse(ptr, prefix, subsys)) != NULL)
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
        char root_prefix[PATH_MAX];
        char *mount = NULL;
        char *root = NULL;
        char *cgroup = NULL;

        pid = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.pid : getppid();
        prefix = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "";

        if (xsnprintf(err, path, sizeof(path), "%s"PROC_MOUNTS_PATH(PROC_PID), prefix, (int32_t)pid) < 0)
                goto fail;
        if ((mount = parse_proc_file(err, path, cgroup_mount, root_prefix, subsys)) == NULL)
                goto fail;
        if (xsnprintf(err, path, sizeof(path), "%s"PROC_CGROUP_PATH(PROC_PID), prefix, (int32_t)cnt->cfg.pid) < 0)
                goto fail;
        if ((root = parse_proc_file(err, path, cgroup_root, root_prefix, subsys)) == NULL)
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
        xasprintf(err, &ns, "%s"PROC_NS_PATH(PROC_PID), prefix, (int32_t)cnt->cfg.pid, namespace);
        return (ns);
}

static int
find_library_paths(struct error *err, struct nvc_container *cnt)
{
        char path[PATH_MAX];
        glob_t gl;
        int rv = -1;
        char **ptr;

        if (!(cnt->flags & OPT_COMPUTE_LIBS))
                return (0);

        if (path_join(err, path, cnt->cfg.rootfs, cnt->cfg.cudart_dir) < 0)
                return (-1);
        if (path_append(err, path, "compat/lib*.so.*") < 0)
                return (-1);

        if (xglob(err, path, GLOB_ERR, NULL, &gl) < 0)
                goto fail;
        if (gl.gl_pathc > 0) {
                cnt->nlibs = gl.gl_pathc;
                cnt->libs = ptr = array_new(err, gl.gl_pathc);
                if (cnt->libs == NULL)
                        goto fail;

                for (size_t i = 0; i < gl.gl_pathc; ++i) {
                        if (path_resolve(err, path, cnt->cfg.rootfs, gl.gl_pathv[i] + strlen(cnt->cfg.rootfs)) < 0)
                                goto fail;
                        if (!str_array_match(path, (const char * const *)cnt->libs, (size_t)(ptr - cnt->libs))) {
                                log_infof("selecting %s%s", cnt->cfg.rootfs, path);
                                if ((*ptr++ = xstrdup(err, path)) == NULL)
                                        goto fail;
                        }
                }
                array_pack(cnt->libs, &cnt->nlibs);
        }
        rv = 0;

 fail:
        globfree(&gl);
        return (rv);
}

static int
lookup_owner(struct error *err, struct nvc_container *cnt)
{
        const char *prefix;
        char path[PATH_MAX];
        struct stat s;

        prefix = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "";
        if (xsnprintf(err, path, sizeof(path), "%s"PROC_PID, prefix, (int32_t)cnt->cfg.pid) < 0)
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
        const char *cudart_dir = cfg->cudart_dir;
        const char *ldconfig = cfg->ldconfig;
        char *rootfs;
        int multiarch, ret;
        int rv = -1;

        cnt->cfg.pid = cfg->pid;
        if ((cnt->cfg.rootfs = xstrdup(err, cfg->rootfs)) == NULL)
                return (-1);

        if (cnt->flags & OPT_STANDALONE) {
                if ((rootfs = xstrdup(err, cnt->cfg.rootfs)) == NULL)
                        return (-1);
        } else {
                if (xsnprintf(err, tmp, sizeof(tmp), PROC_ROOT_PATH(PROC_PID), (int32_t)cnt->cfg.pid) < 0)
                        return (-1);
                if (path_resolve_full(err, path, tmp, cnt->cfg.rootfs) < 0)
                        return (-1);
                if ((rootfs = xstrdup(err, path)) == NULL)
                        return (-1);
        }

        if (bins_dir == NULL)
                bins_dir = USR_BIN_DIR;
        if (libs_dir == NULL || libs32_dir == NULL) {
                /* Debian and its derivatives use a multiarch directory scheme. */
                if (path_resolve_full(err, path, rootfs, "/etc/debian_version") < 0)
                        goto fail;
                if ((multiarch = file_exists(err, path)) < 0)
                        goto fail;

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
                                if (path_resolve_full(err, path, rootfs, USR_LIB32_DIR) < 0)
                                        goto fail;
                                if ((ret = file_exists(err, path)) < 0)
                                        goto fail;
                                if (!ret) {
                                        if (path_resolve_full(err, tmp, rootfs, libs_dir) < 0)
                                                goto fail;
                                        if (path_resolve_full(err, path, rootfs, USR_LIB32_ALT_DIR) < 0)
                                                goto fail;
                                        if ((ret = file_exists(err, path)) < 0)
                                                goto fail;
                                        if (ret && !str_equal(path, tmp))
                                                libs32_dir = USR_LIB32_ALT_DIR;
                                }
                        }
                }
        }
        if (cudart_dir == NULL)
                cudart_dir = CUDA_RUNTIME_DIR;
        if (ldconfig == NULL) {
                /*
                 * Some distributions have a wrapper script around ldconfig to reduce package install time.
                 * Always refer to the real one to prevent having our privileges dropped by a shebang.
                 */
                if (path_resolve_full(err, path, rootfs, LDCONFIG_ALT_PATH) < 0)
                        goto fail;
                if ((ret = file_exists(err, path)) < 0)
                        goto fail;
                ldconfig = ret ? LDCONFIG_ALT_PATH : LDCONFIG_PATH;
        }

        if ((cnt->cfg.bins_dir = xstrdup(err, bins_dir)) == NULL)
                goto fail;
        if ((cnt->cfg.libs_dir = xstrdup(err, libs_dir)) == NULL)
                goto fail;
        if ((cnt->cfg.libs32_dir = xstrdup(err, libs32_dir)) == NULL)
                goto fail;
        if ((cnt->cfg.cudart_dir = xstrdup(err, cudart_dir)) == NULL)
                goto fail;
        if ((cnt->cfg.ldconfig = xstrdup(err, ldconfig)) == NULL)
                goto fail;
        rv = 0;

 fail:
        free(rootfs);
        return (rv);
}

struct nvc_container *
nvc_container_new(struct nvc_context *ctx, const struct nvc_container_config *cfg, const char *opts)
{
        struct nvc_container *cnt;
        int32_t flags;

        if (validate_context(ctx) < 0)
                return (NULL);
        if (validate_args(ctx, cfg != NULL && cfg->pid > 0 && cfg->rootfs != NULL && !str_empty(cfg->rootfs) && cfg->rootfs[0] == '/' &&
            !str_empty(cfg->bins_dir) && !str_empty(cfg->libs_dir) && !str_empty(cfg->libs32_dir) && !str_empty(cfg->cudart_dir) && !str_empty(cfg->ldconfig)) < 0)
                return (NULL);
        if (opts == NULL)
                opts = default_container_opts;
        if ((flags = options_parse(&ctx->err, opts, container_opts, nitems(container_opts))) < 0)
                return (NULL);
        if ((!(flags & OPT_SUPERVISED) ^ !(flags & OPT_STANDALONE)) == 0) {
                error_setx(&ctx->err, "invalid mode of operation");
                return (NULL);
        }

        log_infof("configuring container with '%s'", opts);
        if ((cnt = xcalloc(&ctx->err, 1, sizeof(*cnt))) == NULL)
                return (NULL);

        cnt->flags = flags;
        if (copy_config(&ctx->err, cnt, cfg) < 0)
                goto fail;
        if (lookup_owner(&ctx->err, cnt) < 0)
                goto fail;
        if (!(flags & OPT_NO_CNTLIBS)) {
                if (find_library_paths(&ctx->err, cnt) < 0)
                        goto fail;
        }
        if ((cnt->mnt_ns = find_namespace_path(&ctx->err, cnt, "mnt")) == NULL)
                goto fail;
        if (!(flags & OPT_NO_CGROUPS)) {
                if ((cnt->dev_cg = find_cgroup_path(&ctx->err, cnt, "devices")) == NULL)
                        goto fail;
        }

        log_infof("setting pid to %"PRId32, (int32_t)cnt->cfg.pid);
        log_infof("setting rootfs to %s", cnt->cfg.rootfs);
        log_infof("setting owner to %"PRIu32":%"PRIu32, (uint32_t)cnt->uid, (uint32_t)cnt->gid);
        log_infof("setting bins directory to %s", cnt->cfg.bins_dir);
        log_infof("setting libs directory to %s", cnt->cfg.libs_dir);
        log_infof("setting libs32 directory to %s", cnt->cfg.libs32_dir);
        log_infof("setting cudart directory to %s", cnt->cfg.cudart_dir);
        log_infof("setting ldconfig to %s%s", cnt->cfg.ldconfig, (cnt->cfg.ldconfig[0] == '@') ? " (host relative)" : "");
        log_infof("setting mount namespace to %s", cnt->mnt_ns);
        if (!(flags & OPT_NO_CGROUPS))
                log_infof("setting devices cgroup to %s", cnt->dev_cg);
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
        free(cnt->cfg.cudart_dir);
        free(cnt->cfg.ldconfig);
        free(cnt->mnt_ns);
        free(cnt->dev_cg);
        array_free(cnt->libs, cnt->nlibs);
        free(cnt);
}
