/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <sys/types.h>

#include <errno.h>
#include <libgen.h>
#undef basename /* Use the GNU version of basename. */
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

static char **mount_files(struct error *, const char *, const struct nvc_container *, const char *, char *[], size_t);
static char *mount_device(struct error *, const char *, const struct nvc_container *, const struct nvc_device_node *);
static char *mount_ipc(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_procfs(struct error *, const char *, const struct nvc_container *);
static char *mount_procfs_gpu(struct error *, const char *, const struct nvc_container *, const char *);
static char *mount_app_profile(struct error *, const struct nvc_container *);
static int  update_app_profile(struct error *, const struct nvc_container *, dev_t);
static void unmount(const char *);
static int  setup_cgroup(struct error *, const char *, dev_t);
static int  symlink_library(struct error *, const char *, const char *, const char *, uid_t, gid_t);
static int  symlink_libraries(struct error *, const struct nvc_container *, const char * const [], size_t);
static void filter_libraries(const struct nvc_driver_info *, char * [], size_t *);

static char **
mount_files(struct error *err, const char *root, const struct nvc_container *cnt, const char *dir, char *paths[], size_t size)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char *src_end, *dst_end, *file;
        char **mnt, **ptr;

        if (path_new(err, src, root) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, dir) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, MODE_DIR(0755)) < 0)
                return (NULL);
        src_end = src + strlen(src);
        dst_end = dst + strlen(dst);

        mnt = ptr = array_new(err, size + 1); /* NULL terminated. */
        if (mnt == NULL)
                return (NULL);

        for (size_t i = 0; i < size; ++i) {
                file = basename(paths[i]);
                if (!match_binary_flags(file, cnt->flags) && !match_library_flags(file, cnt->flags))
                        continue;
                if (path_append(err, src, paths[i]) < 0)
                        goto fail;
                if (path_append(err, dst, file) < 0)
                        goto fail;
                if (file_mode(err, src, &mode) < 0)
                        goto fail;
                if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                        goto fail;

                log_infof("mounting %s at %s", src, dst);
                if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                        goto fail;
                if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NODEV|MS_NOSUID, NULL) < 0)
                        goto fail;
                if ((*ptr++ = xstrdup(err, dst)) == NULL)
                        goto fail;
                *src_end = '\0';
                *dst_end = '\0';
        }
        return (mnt);

 fail:
        for (size_t i = 0; i < size; ++i)
                unmount(mnt[i]);
        array_free(mnt, size);
        return (NULL);
}

static char *
mount_device(struct error *err, const char *root, const struct nvc_container *cnt, const struct nvc_device_node *dev)
{
        struct stat s;
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char *mnt;

        if (path_join(err, src, root, dev->path) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, dev->path) < 0)
                return (NULL);
        if (xstat(err, src, &s) < 0)
                return (NULL);
        if (s.st_rdev != dev->id) {
                error_setx(err, "invalid device node: %s", src);
                return (NULL);
        }
        if (file_mode(err, src, &mode) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                return (NULL);

        log_infof("mounting %s at %s", src, dst);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(dst);
        return (NULL);
}

static char *
mount_ipc(struct error *err, const char *root, const struct nvc_container *cnt, const char *ipc)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        mode_t mode;
        char *mnt;

        if (path_join(err, src, root, ipc) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, ipc) < 0)
                return (NULL);
        if (file_mode(err, src, &mode) < 0)
                return (NULL);
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                return (NULL);

        log_infof("mounting %s at %s", src, dst);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(dst);
        return (NULL);
}

static char *
mount_app_profile(struct error *err, const struct nvc_container *cnt)
{
        char path[PATH_MAX];
        char *mnt;

        if (path_resolve_full(err, path, cnt->cfg.rootfs, NV_APP_PROFILE_DIR) < 0)
                return (NULL);
        if (file_create(err, path, NULL, cnt->uid, cnt->gid, MODE_DIR(0555)) < 0)
                return (NULL);

        log_infof("mounting tmpfs at %s", path);
        if (xmount(err, "tmpfs", path, "tmpfs", 0, "mode=0555") < 0)
                goto fail;
        /* XXX Some kernels require MS_BIND in order to remount within a userns */
        if (xmount(err, NULL, path, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, path)) == NULL)
                goto fail;
        return (mnt);

 fail:
        unmount(path);
        return (NULL);
}

static int
update_app_profile(struct error *err, const struct nvc_container *cnt, dev_t id)
{
        char path[PATH_MAX];
        char *buf = NULL;
        char *ptr;
        uintmax_t n;
        uint64_t dev;
        int rv = -1;

#define profile quote_str({\
        "profiles": [{"name": "_container_", "settings": ["EGLVisibleDGPUDevices", 0x%lx]}],\
        "rules": [{"pattern": [], "profile": "_container_"}]\
})

        dev = 1ull << minor(id);
        if (path_resolve_full(err, path, cnt->cfg.rootfs, NV_APP_PROFILE_DIR "/10-container.conf") < 0)
                return (-1);
        if (file_read_text(err, path, &buf) < 0) {
                if (err->code != ENOENT)
                        goto fail;
                if (xasprintf(err, &buf, profile, dev) < 0)
                        goto fail;
        } else {
                if ((ptr = strstr(buf, "0x")) == NULL ||
                    (n = strtoumax(ptr, NULL, 16)) == UINTMAX_MAX) {
                        error_setx(err, "invalid application profile: %s", path);
                        goto fail;
                }
                free(buf), buf = NULL;
                if (xasprintf(err, &buf, profile, (uint64_t)n|dev) < 0)
                        goto fail;
        }
        if (file_create(err, path, buf, cnt->uid, cnt->gid, MODE_REG(0555)) < 0)
                goto fail;
        rv = 0;

#undef profile

 fail:
        free(buf);
        return (rv);
}

static char *
mount_procfs(struct error *err, const char *root, const struct nvc_container *cnt)
{
        char src[PATH_MAX];
        char dst[PATH_MAX];
        char *src_end, *dst_end, *mnt, *param;
        mode_t mode;
        char *buf = NULL;
        const char *files[] = {
                "params",
                "version",
                "registry",
        };

        if (path_join(err, src, root, NV_PROC_DRIVER) < 0)
                return (NULL);
        if (path_resolve_full(err, dst, cnt->cfg.rootfs, NV_PROC_DRIVER) < 0)
                return (NULL);
        src_end = src + strlen(src);
        dst_end = dst + strlen(dst);

        log_infof("mounting tmpfs at %s", dst);
        if (xmount(err, "tmpfs", dst, "tmpfs", 0, "mode=0555") < 0)
                return (NULL);

        for (size_t i = 0; i < nitems(files); ++i) {
                if (path_append(err, src, files[i]) < 0)
                        goto fail;
                if (path_append(err, dst, files[i]) < 0)
                        goto fail;
                if (file_mode(err, src, &mode) < 0) {
                        if (err->code == ENOENT)
                                continue;
                        goto fail;
                }
                if (file_read_text(err, src, &buf) < 0)
                        goto fail;
                /* Prevent NVRM from adjusting the device nodes. */
                if (i == 0 && (param = strstr(buf, "ModifyDeviceFiles: 1")) != NULL)
                        param[19] = '0';
                if (file_create(err, dst, buf, cnt->uid, cnt->gid, mode) < 0)
                        goto fail;
                *src_end = '\0';
                *dst_end = '\0';
                free(buf);
                buf = NULL;
        }
        /* XXX Some kernels require MS_BIND in order to remount within a userns */
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        return (mnt);

 fail:
        *dst_end = '\0';
        unmount(dst);
        free(buf);
        return (NULL);
}

static char *
mount_procfs_gpu(struct error *err, const char *root, const struct nvc_container *cnt, const char *busid)
{
        char src[PATH_MAX];
        char dst[PATH_MAX] = {0};
        char *gpu = NULL;
        char *mnt = NULL;
        mode_t mode;

        for (int off = 0;; off += 4) {
                /* XXX Check if the driver procfs uses 32-bit or 16-bit PCI domain */
                if (xasprintf(err, &gpu, "%s/gpus/%s", NV_PROC_DRIVER, busid + off) < 0)
                        return (NULL);
                if (path_join(err, src, root, gpu) < 0)
                        goto fail;
                if (path_resolve_full(err, dst, cnt->cfg.rootfs, gpu) < 0)
                        goto fail;
                if (file_mode(err, src, &mode) == 0)
                        break;
                if (err->code != ENOENT || off != 0)
                        goto fail;
                *dst = '\0';
                free(gpu);
                gpu = NULL;
        }
        if (file_create(err, dst, NULL, cnt->uid, cnt->gid, mode) < 0)
                goto fail;

        log_infof("mounting %s at %s", src, dst);
        if (xmount(err, src, dst, NULL, MS_BIND, NULL) < 0)
                goto fail;
        if (xmount(err, NULL, dst, NULL, MS_BIND|MS_REMOUNT | MS_RDONLY|MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) < 0)
                goto fail;
        if ((mnt = xstrdup(err, dst)) == NULL)
                goto fail;
        free(gpu);
        return (mnt);

 fail:
        free(gpu);
        unmount(dst);
        return (NULL);
}

static void
unmount(const char *path)
{
        if (path == NULL || str_empty(path))
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

        if (path_join(err, path, cgroup, "devices.allow") < 0)
                return (-1);
        if ((fs = xfopen(err, path, "a")) == NULL)
                return (-1);

        log_infof("whitelisting device node %u:%u", major(id), minor(id));
        /* XXX dprintf doesn't seem to catch the write errors, flush the stream explicitly instead. */
        if (fprintf(fs, "c %u:%u rw", major(id), minor(id)) < 0 || fflush(fs) == EOF || ferror(fs)) {
                error_set(err, "write error: %s", path);
                goto fail;
        }
        rv = 0;

 fail:
        fclose(fs);
        return (rv);
}

static int
symlink_library(struct error *err, const char *src, const char *target, const char *linkname, uid_t uid, gid_t gid)
{
        char path[PATH_MAX];
        char *tmp;
        int rv = -1;

        if ((tmp = xstrdup(err, src)) == NULL)
                return (-1);
        if (path_join(err, path, dirname(tmp), linkname) < 0)
                goto fail;

        log_infof("creating symlink %s -> %s", path, target);
        if (file_create(err, path, target, uid, gid, MODE_LNK(0777)) < 0)
                goto fail;
        rv = 0;

 fail:
        free(tmp);
        return (rv);
}

static int
symlink_libraries(struct error *err, const struct nvc_container *cnt, const char * const paths[], size_t size)
{
        char *lib;

        for (size_t i = 0; i < size; ++i) {
                lib = basename(paths[i]);
                if (str_has_prefix(lib, "libcuda.so")) {
                        /* XXX Many applications wrongly assume that libcuda.so exists (e.g. with dlopen). */
                        if (symlink_library(err, paths[i], SONAME_LIBCUDA, "libcuda.so", cnt->uid, cnt->gid) < 0)
                                return (-1);
                } else if (str_has_prefix(lib, "libGLX_nvidia.so")) {
                        /* XXX GLVND requires this symlink for indirect GLX support. */
                        if (symlink_library(err, paths[i], lib, "libGLX_indirect.so.0", cnt->uid, cnt->gid) < 0)
                                return (-1);
                } else if (str_has_prefix(lib, "libnvidia-opticalflow.so")) {
                        /* XXX Fix missing symlink for libnvidia-opticalflow.so. */
                        if (symlink_library(err, paths[i], "libnvidia-opticalflow.so.1", "libnvidia-opticalflow.so", cnt->uid, cnt->gid) < 0)
                                return (-1);
                }
        }
        return (0);
}

static void
filter_libraries(const struct nvc_driver_info *info, char * paths[], size_t *size)
{
        char *lib, *maj;

        /*
         * XXX Filter out any library that matches the major version of RM to prevent us from
         * running into an unsupported configurations (e.g. CUDA compat on Geforce or non-LTS drivers).
         */
        for (size_t i = 0; i < *size; ++i) {
                lib = basename(paths[i]);
                if ((maj = strstr(lib, ".so.")) != NULL) {
                        maj += strlen(".so.");
                        if (strncmp(info->nvrm_version, maj, strspn(maj, "0123456789")))
                                continue;
                }
                paths[i] = NULL;
        }
        array_pack(paths, size);
}

int
nvc_driver_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_driver_info *info)
{
        const char **mnt, **ptr, **tmp;
        size_t nmnt;
        int rv = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && info != NULL) < 0)
                return (-1);

        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        nmnt = 2 + info->nbins + info->nlibs + cnt->nlibs + info->nlibs32 + info->nipcs + info->ndevs;
        mnt = ptr = (const char **)array_new(&ctx->err, nmnt);
        if (mnt == NULL)
                goto fail;

        /* Procfs mount */
        if ((*ptr++ = mount_procfs(&ctx->err, ctx->cfg.root, cnt)) == NULL)
                goto fail;

        /* Application profile mount */
        if (cnt->flags & OPT_GRAPHICS_LIBS) {
                if ((*ptr++ = mount_app_profile(&ctx->err, cnt)) == NULL)
                        goto fail;
        }

        /* Host binary and library mounts */
        if (info->bins != NULL && info->nbins > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, ctx->cfg.root, cnt, cnt->cfg.bins_dir, info->bins, info->nbins)) == NULL)
                        goto fail;
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }
        if (info->libs != NULL && info->nlibs > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, ctx->cfg.root, cnt, cnt->cfg.libs_dir, info->libs, info->nlibs)) == NULL)
                        goto fail;
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }
        if ((cnt->flags & OPT_COMPAT32) && info->libs32 != NULL && info->nlibs32 > 0) {
                if ((tmp = (const char **)mount_files(&ctx->err, ctx->cfg.root, cnt, cnt->cfg.libs32_dir, info->libs32, info->nlibs32)) == NULL)
                        goto fail;
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
        }
        if (symlink_libraries(&ctx->err, cnt, mnt, (size_t)(ptr - mnt)) < 0)
                goto fail;

        /* Container library mounts */
        if (cnt->libs != NULL && cnt->nlibs > 0) {
                size_t nlibs = cnt->nlibs;
                char **libs = array_copy(&ctx->err, (const char * const *)cnt->libs, cnt->nlibs);
                if (libs == NULL)
                        goto fail;

                filter_libraries(info, libs, &nlibs);
                if ((tmp = (const char **)mount_files(&ctx->err, cnt->cfg.rootfs, cnt, cnt->cfg.libs_dir, libs, nlibs)) == NULL) {
                        free(libs);
                        goto fail;
                }
                ptr = array_append(ptr, tmp, array_size(tmp));
                free(tmp);
                free(libs);
        }

        /* IPC mounts */
        for (size_t i = 0; i < info->nipcs; ++i) {
                /* XXX Only utility libraries require persistenced IPC, everything else is compute only. */
                if (str_has_suffix(NV_PERSISTENCED_SOCKET, info->ipcs[i])) {
                        if (!(cnt->flags & OPT_UTILITY_LIBS))
                                continue;
                } else if (!(cnt->flags & OPT_COMPUTE_LIBS))
                        continue;
                if ((*ptr++ = mount_ipc(&ctx->err, ctx->cfg.root, cnt, info->ipcs[i])) == NULL)
                        goto fail;
        }

        /* Device mounts */
        for (size_t i = 0; i < info->ndevs; ++i) {
                /* XXX Only compute libraries require specific devices (e.g. UVM). */
                if (!(cnt->flags & OPT_COMPUTE_LIBS) && major(info->devs[i].id) != NV_DEVICE_MAJOR)
                        continue;
                /* XXX Only display capability requires the modeset device. */
                if (!(cnt->flags & OPT_DISPLAY) && minor(info->devs[i].id) == NV_MODESET_DEVICE_MINOR)
                        continue;
                if (!(cnt->flags & OPT_NO_DEVBIND)) {
                        if ((*ptr++ = mount_device(&ctx->err, ctx->cfg.root, cnt, &info->devs[i])) == NULL)
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
                for (size_t i = 0; mnt != NULL && i < nmnt; ++i)
                        unmount(mnt[i]);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        array_free((char **)mnt, nmnt);
        return (rv);
}

int
nvc_device_mount(struct nvc_context *ctx, const struct nvc_container *cnt, const struct nvc_device *dev)
{
        char *dev_mnt = NULL;
        char *proc_mnt = NULL;
        int rv = -1;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL && dev != NULL) < 0)
                return (-1);

        if (ns_enter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                return (-1);

        if (!(cnt->flags & OPT_NO_DEVBIND)) {
                if ((dev_mnt = mount_device(&ctx->err, ctx->cfg.root, cnt, &dev->node)) == NULL)
                        goto fail;
        }
        if ((proc_mnt = mount_procfs_gpu(&ctx->err, ctx->cfg.root, cnt, dev->busid)) == NULL)
                goto fail;
        if (cnt->flags & OPT_GRAPHICS_LIBS) {
                if (update_app_profile(&ctx->err, cnt, dev->node.id) < 0)
                        goto fail;
        }
        if (!(cnt->flags & OPT_NO_CGROUPS)) {
                if (setup_cgroup(&ctx->err, cnt->dev_cg, dev->node.id) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        if (rv < 0) {
                unmount(proc_mnt);
                unmount(dev_mnt);
                assert_func(ns_enter_at(NULL, ctx->mnt_ns, CLONE_NEWNS));
        } else {
                rv = ns_enter_at(&ctx->err, ctx->mnt_ns, CLONE_NEWNS);
        }

        free(proc_mnt);
        free(dev_mnt);
        return (rv);
}
