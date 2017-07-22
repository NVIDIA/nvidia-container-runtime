/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/mount.h>
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

#define CGROUP_DEVICE_ALLOW "devices.allow"
#define CGROUP_DEVICE_STR   "c %u:%u %s"

static char *mount_files(struct error *, const struct nvc_container *, const char *, char *[], size_t);
static char *mount_device(struct error *, const struct nvc_container *, const char *);
static char *mount_ipc(struct error *, const struct nvc_container *, const char *);
static void unmount(const char *);
static int  setup_cgroup(struct error *, const char *, dev_t);
static int  symlink_library(struct error *, const char *, const char *, const char *, const char *, uid_t, gid_t);
static int  symlink_libraries(struct error *, const struct nvc_container *, const char *, char *[], size_t, const char *);

static char *
mount_files(struct error *err, const struct nvc_container *cnt, const char *dir, char *paths[], size_t size)
{
        char path[PATH_MAX];
        mode_t mode;
        char *ptr, *mnt;

        /* Create the top directory under the rootfs. */
        if (path_resolve(err, path, cnt->cfg.rootfs, dir) < 0)
                return (NULL);
        if (file_create(err, path, NULL, cnt->uid, cnt->gid, MODE_DIR(0755)) < 0)
                return (NULL);

        ptr = path + strlen(path);

        /* Bind mount the top directory with slave propagation. */
        if (xmount(err, path, path, NULL, MS_BIND|MS_REC, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, path, NULL, MS_BIND|MS_REC|MS_REMOUNT | MS_NODEV|MS_NOSUID, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, path, NULL, MS_SLAVE|MS_REC, NULL) < 0)
                goto fail;

        /* Bind mount every files under the top directory with read-only permissions. */
        for (size_t i = 0; i < size; ++i) {
                if (path_append(err, path, basename(paths[i])) < 0)
                        goto fail;
                if (file_mode(err, paths[i], &mode) < 0)
                        goto fail;
                if (file_create(err, path, NULL, cnt->uid, cnt->gid, mode) < 0)
                        goto fail;

                log_infof("mounting %s at %s", paths[i], path);
                if (xmount(err, paths[i], path, NULL, MS_BIND, NULL) < 0)
                        goto fail;
                if (xmount(err, NULL, path, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NODEV|MS_NOSUID, NULL) < 0)
                        goto fail;
                *ptr = '\0';
        }
        if ((mnt = xstrdup(err, path)) == NULL)
                goto fail;
        return (mnt);

 fail:
        *ptr = '\0';
        unmount(path);
        return (NULL);
}

static char *
mount_device(struct error *err, const struct nvc_container *cnt, const char *dev)
{
        char path[PATH_MAX];
        mode_t mode;
        char *mnt;

        if (path_resolve(err, path, cnt->cfg.rootfs, dev) < 0)
                return (NULL);
        if (file_mode(err, dev, &mode) < 0)
                return (NULL);
        if (file_create(err, path, NULL, cnt->uid, cnt->gid, mode) < 0)
                return (NULL);

        log_infof("mounting %s at %s", dev, path);
        if (xmount(err, dev, path, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, path, NULL, MS_BIND|MS_REMOUNT | MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, path)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(path);
        return (NULL);
}

static char *
mount_ipc(struct error *err, const struct nvc_container *cnt, const char *ipc)
{
        char path[PATH_MAX];
        mode_t mode;
        char *mnt;

        if (path_resolve(err, path, cnt->cfg.rootfs, ipc) < 0)
                return (NULL);
        if (file_mode(err, ipc, &mode) < 0)
                return (NULL);
        if (file_create(err, path, NULL, cnt->uid, cnt->gid, mode) < 0)
                return (NULL);

        log_infof("mounting %s at %s", ipc, path);
        if (xmount(err, ipc, path, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, path, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, path)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(path);
        return (NULL);
}

static void
unmount(const char *path)
{
        if (path == NULL)
                return;
        umount2(path, MNT_DETACH);
        file_remove(NULL, path);
}

static int
setup_cgroup(struct error *err, const char *cgroup, dev_t id)
{
        char path[PATH_MAX];
        FILE *fs;
        int rv = -1;

        if (path_join(err, path, cgroup, CGROUP_DEVICE_ALLOW) < 0)
                return (-1);
        if ((fs = xfopen(err, path, "a")) == NULL)
                return (-1);

        log_infof("whitelisting device node %u:%u", major(id), minor(id));
        if (fprintf(fs, CGROUP_DEVICE_STR, major(id), minor(id), "rwm") < 0 || ferror(fs)) {
                error_set(err, "write error: %s", path);
                goto fail;
        }
        rv = 0;

 fail:
        fclose(fs);
        return (rv);
}

static int
symlink_library(struct error *err, const char *dir, const char *lib, const char *version, const char *linkname, uid_t uid, gid_t gid)
{
        char path[PATH_MAX];
        char *target;
        int rv = -1;

        if (path_join(err, path, dir, linkname) < 0)
                return (-1);
        if (xasprintf(err, &target, "%s.%s", lib, version) < 0)
                return (-1);

        log_infof("creating symlink %s -> %s", path, target);
        if (file_create(err, path, target, uid, gid, MODE_LNK(0777)) < 0)
                goto fail;
        rv = 0;

 fail:
        free(target);
        return (rv);
}

static int
symlink_libraries(struct error *err, const struct nvc_container *cnt, const char *dir, char *paths[], size_t size, const char *version)
{
        char *p;

        for (size_t i = 0; i < size; ++i) {
                p = basename(paths[i]);
                if (!strpcmp(p, "libcuda.so")) {
                        /* XXX Many applications wrongly assume that libcuda.so exists (e.g. with dlopen). */
                        if (symlink_library(err, dir, "libcuda.so", version, "libcuda.so", cnt->uid, cnt->gid) < 0)
                                return (-1);
                } else if (!strpcmp(p, "libGLX_nvidia.so")) {
                        /* XXX GLVND requires this symlink for indirect GLX support. */
                        if (symlink_library(err, dir, "libGLX_nvidia.so", version, "libGLX_indirect.so.0", cnt->uid, cnt->gid) < 0)
                                return (-1);
                }
        }
        return (0);
}

int
nvc_driver_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_driver_info *info)
{
        char **files_mnt = NULL;
        char **ipcs_mnt = NULL;
        char **devs_mnt = NULL;
        size_t nfiles_mnt = 0;
        size_t nipcs_mnt = 0;
        size_t ndevs_mnt = 0;
        char **mnt;
        int rv = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && info != NULL) < 0)
                return (-1);

        if (nsenter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        /* File mounts */
        nfiles_mnt = 3;
        files_mnt = mnt = array_new(&ctx->err, nfiles_mnt);
        if (files_mnt == NULL)
                goto fail;
        if (info->bins != NULL && info->nbins > 0) {
                if ((*mnt = mount_files(&ctx->err, cnt, cnt->cfg.bins_dir, info->bins, info->nbins)) == NULL)
                        goto fail;
                ++mnt;
        }
        if (info->libs != NULL && info->nlibs > 0) {
                if ((*mnt = mount_files(&ctx->err, cnt, cnt->cfg.libs_dir, info->libs, info->nlibs)) == NULL)
                        goto fail;
                if (symlink_libraries(&ctx->err, cnt, *mnt, info->libs, info->nlibs, info->nvrm_version) < 0)
                        goto fail;
                ++mnt;
        }
        if (info->libs32 != NULL && info->nlibs32 > 0) {
                if ((*mnt = mount_files(&ctx->err, cnt, cnt->cfg.libs32_dir, info->libs32, info->nlibs32)) == NULL)
                        goto fail;
                if (symlink_libraries(&ctx->err, cnt, *mnt, info->libs32, info->nlibs32, info->nvrm_version) < 0)
                        goto fail;
                ++mnt;
        }

        /* IPC mounts */
        nipcs_mnt = info->nipcs;
        ipcs_mnt = mnt = array_new(&ctx->err, nipcs_mnt);
        if (ipcs_mnt == NULL)
                goto fail;
        for (size_t i = 0; i < nipcs_mnt; ++i, ++mnt) {
                if ((*mnt = mount_ipc(&ctx->err, cnt, info->ipcs[i])) == NULL)
                        goto fail;
        }

        /* Device mounts */
        ndevs_mnt = info->ndevs;
        devs_mnt = mnt = array_new(&ctx->err, ndevs_mnt);
        if (devs_mnt == NULL)
                goto fail;
        for (size_t i = 0; i < ndevs_mnt; ++i, ++mnt) {
                if (!(cnt->flags & OPT_NO_DEVBIND)) {
                        if ((*mnt = mount_device(&ctx->err, cnt, info->devs[i].path)) == NULL)
                                goto fail;
                }
                if (!(cnt->flags & OPT_NO_CGROUPS)) {
                        if (setup_cgroup(&ctx->err, cnt->dev_cg, info->devs[i].id) < 0)
                                goto fail;
                }
        }
        rv = 0;

 fail:
        if (rv < 0) {
                for (size_t i = 0; files_mnt != NULL && i < nfiles_mnt; ++i)
                        unmount(files_mnt[i]);
                for (size_t i = 0; ipcs_mnt != NULL && i < nipcs_mnt; ++i)
                        unmount(ipcs_mnt[i]);
                for (size_t i = 0; devs_mnt != NULL && i < ndevs_mnt; ++i)
                        unmount(devs_mnt[i]);
                assert_func(nsenterat(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = nsenterat(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        array_free(files_mnt, nfiles_mnt);
        array_free(ipcs_mnt, nipcs_mnt);
        array_free(devs_mnt, ndevs_mnt);
        return (rv);
}

int
nvc_device_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_device *dev)
{
        char *mnt = NULL;
        int rv = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && dev != NULL) < 0)
                return (-1);

        if (nsenter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        if (!(cnt->flags & OPT_NO_DEVBIND)) {
                if ((mnt = mount_device(&ctx->err, cnt, dev->node.path)) == NULL)
                        goto fail;
        }
        if (!(cnt->flags & OPT_NO_CGROUPS)) {
                if (setup_cgroup(&ctx->err, cnt->dev_cg, dev->node.id) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        if (rv < 0) {
                unmount(mnt);
                assert_func(nsenterat(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = nsenterat(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        free(mnt);
        return (rv);
}
