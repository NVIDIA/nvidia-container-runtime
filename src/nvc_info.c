/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/sysmacros.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "driver.h"
#include "elftool.h"
#include "error.h"
#include "ldcache.h"
#include "options.h"
#include "utils.h"
#include "xfuncs.h"

#define MAX_BINS (nitems(utility_bins) + \
                  nitems(compute_bins))
#define MAX_LIBS (nitems(utility_libs) + \
                  nitems(compute_libs) + \
                  nitems(video_libs) + \
                  nitems(graphics_libs) + \
                  nitems(graphics_libs_glvnd) + \
                  nitems(graphics_libs_compat))

static int select_libraries(struct error *, void *, const char *, const char *, const char *);
static int find_library_paths(struct error *, struct nvc_driver_info *, const char *, const char *, const char * const [], size_t);
static int find_binary_paths(struct error *, struct nvc_driver_info *, const char *, const char * const [], size_t);
static int find_device_node(struct error *, const char *, const char *, struct nvc_device_node *);
static int find_ipc_path(struct error *, const char *, const char *, char **);
static int lookup_libraries(struct error *, struct nvc_driver_info *, const char *, int32_t, const char *);
static int lookup_binaries(struct error *, struct nvc_driver_info *, const char *, int32_t);
static int lookup_devices(struct error *, struct nvc_driver_info *, const char *, int32_t);
static int lookup_ipcs(struct error *, struct nvc_driver_info *, const char *, int32_t);

/*
 * Display libraries are not needed.
 *
 * "libnvidia-gtk2.so" // GTK2 (used by nvidia-settings)
 * "libnvidia-gtk3.so" // GTK3 (used by nvidia-settings)
 * "libnvidia-wfb.so"  // Wrapped software rendering module for X server
 * "nvidia_drv.so"     // Driver module for X server
 * "libglx.so"         // GLX extension module for X server
 */

static const char * const utility_bins[] = {
        "nvidia-smi",                       /* System management interface */
        "nvidia-debugdump",                 /* GPU coredump utility */
        "nvidia-persistenced",              /* Persistence mode utility */
        //"nvidia-modprobe",                /* Kernel module loader */
        //"nvidia-settings",                /* X server settings */
        //"nvidia-xconfig",                 /* X xorg.conf editor */
};

static const char * const compute_bins[] = {
        "nvidia-cuda-mps-control",          /* Multi process service CLI */
        "nvidia-cuda-mps-server",           /* Multi process service server */
};

static const char * const utility_libs[] = {
        "libnvidia-ml.so",                  /* Management library */
        "libnvidia-cfg.so",                 /* GPU configuration */
};

static const char * const compute_libs[] = {
        "libcuda.so",                       /* CUDA driver library */
        "libnvidia-opencl.so",              /* NVIDIA OpenCL ICD */
        "libnvidia-ptxjitcompiler.so",      /* PTX-SASS JIT compiler (used by libcuda) */
        "libnvidia-fatbinaryloader.so",     /* fatbin loader (used by libcuda) */
        "libnvidia-compiler.so",            /* NVVM-PTX compiler for OpenCL (used by libnvidia-opencl) */
};

static const char * const video_libs[] = {
        "libvdpau_nvidia.so",               /* NVIDIA VDPAU ICD */
        "libnvidia-encode.so",              /* Video encoder */
        "libnvidia-opticalflow.so",         /* NVIDIA Opticalflow library */
        "libnvcuvid.so",                    /* Video decoder */
};

static const char * const graphics_libs[] = {
        //"libnvidia-egl-wayland.so",       /* EGL wayland platform extension (used by libEGL_nvidia) */
        "libnvidia-eglcore.so",             /* EGL core (used by libGLES*[_nvidia] and libEGL_nvidia) */
        "libnvidia-glcore.so",              /* OpenGL core (used by libGL or libGLX_nvidia) */
        "libnvidia-tls.so",                 /* Thread local storage (used by libGL or libGLX_nvidia) */
        "libnvidia-glsi.so",                /* OpenGL system interaction (used by libEGL_nvidia) */
        "libnvidia-fbc.so",                 /* Framebuffer capture */
        "libnvidia-ifr.so",                 /* OpenGL framebuffer capture */
};

static const char * const graphics_libs_glvnd[] = {
        //"libGLX.so",                      /* GLX ICD loader */
        //"libOpenGL.so",                   /* OpenGL ICD loader */
        //"libGLdispatch.so",               /* OpenGL dispatch (used by libOpenGL, libEGL and libGLES*) */
        "libGLX_nvidia.so",                 /* OpenGL/GLX ICD */
        "libEGL_nvidia.so",                 /* EGL ICD */
        "libGLESv2_nvidia.so",              /* OpenGL ES v2 ICD */
        "libGLESv1_CM_nvidia.so",           /* OpenGL ES v1 common profile ICD */
	"libnvidia-glvkspirv.so",           /* SPIR-V Lib for Vulkan */
};

static const char * const graphics_libs_compat[] = {
        "libGL.so",                         /* OpenGL/GLX legacy _or_ compatibility wrapper (GLVND) */
        "libEGL.so",                        /* EGL legacy _or_ ICD loader (GLVND) */
        "libGLESv1_CM.so",                  /* OpenGL ES v1 common profile legacy _or_ ICD loader (GLVND) */
        "libGLESv2.so",                     /* OpenGL ES v2 legacy _or_ ICD loader (GLVND) */
};

static int
select_libraries(struct error *err, void *ptr, const char *root, const char *orig_path, const char *alt_path)
{
        char path[PATH_MAX];
        struct nvc_driver_info *info = ptr;
        struct elftool et;
        char *lib;
        int rv = true;

        if (path_join(err, path, root, alt_path) < 0)
                return (-1);
        elftool_init(&et, err);
        if (elftool_open(&et, path) < 0)
                return (-1);

        lib = basename(alt_path);
        if (str_has_prefix(lib, "libnvidia-tls.so")) {
                /* Only choose the TLS library using the new ABI (kernel 2.3.99). */
                if ((rv = elftool_has_abi(&et, (uint32_t[3]){0x02, 0x03, 0x63})) != true)
                        goto done;
        }
        /* Check the driver version. */
        if ((rv = str_has_suffix(lib, info->nvrm_version)) == false)
                goto done;
        if (str_array_match_prefix(lib, graphics_libs_compat, nitems(graphics_libs_compat))) {
                /* Only choose OpenGL/EGL libraries issued by NVIDIA. */
                if ((rv = elftool_has_dependency(&et, "libnvidia-glcore.so")) != false)
                        goto done;
                if ((rv = elftool_has_dependency(&et, "libnvidia-eglcore.so")) != false)
                        goto done;
        }

 done:
        if (rv)
                log_infof((orig_path == NULL) ? "%s %s" : "%s %s over %s", "selecting", alt_path, orig_path);
        else
                log_infof("skipping %s", alt_path);

        elftool_close(&et);
        return (rv);
}

static int
find_library_paths(struct error *err, struct nvc_driver_info *info, const char *root,
    const char *ldcache, const char * const libs[], size_t size)
{
        char path[PATH_MAX];
        struct ldcache ld;
        int rv = -1;

        if (path_resolve_full(err, path, root, ldcache) < 0)
                return (-1);
        ldcache_init(&ld, err, path);
        if (ldcache_open(&ld) < 0)
                return (-1);

        info->nlibs = size;
        info->libs = array_new(err, size);
        if (info->libs == NULL)
                goto fail;
        if (ldcache_resolve(&ld, LIB_ARCH, root, libs,
            info->libs, info->nlibs, select_libraries, info) < 0)
                goto fail;

        info->nlibs32 = size;
        info->libs32 = array_new(err, size);
        if (info->libs32 == NULL)
                goto fail;
        if (ldcache_resolve(&ld, LIB32_ARCH, root, libs,
            info->libs32, info->nlibs32, select_libraries, info) < 0)
                goto fail;
        rv = 0;

 fail:
        if (ldcache_close(&ld) < 0)
                return (-1);
        return (rv);
}

static int
find_binary_paths(struct error *err, struct nvc_driver_info *info, const char *root,
    const char * const bins[], size_t size)
{
        char *env, *ptr;
        const char *dir;
        char tmp[PATH_MAX];
        char path[PATH_MAX];
        int rv = -1;

        if ((env = secure_getenv("PATH")) == NULL) {
                error_setx(err, "environment variable PATH not found");
                return (-1);
        }
        if ((env = ptr = xstrdup(err, env)) == NULL)
                return (-1);

        info->nbins = size;
        info->bins = array_new(err, size);
        if (info->bins == NULL)
                goto fail;

        while ((dir = strsep(&ptr, ":")) != NULL) {
                if (*dir == '\0')
                        dir = ".";
                for (size_t i = 0; i < size; ++i) {
                        if (info->bins[i] != NULL)
                                continue;
                        if (path_join(NULL, tmp, dir, bins[i]) < 0)
                                continue;
                        if (path_resolve(NULL, path, root, tmp) < 0)
                                continue;
                        if (file_exists_at(NULL, root, path) == true) {
                                info->bins[i] = xstrdup(err, path);
                                if (info->bins[i] == NULL)
                                        goto fail;
                                log_infof("selecting %s", path);
                        }
                }
        }
        rv = 0;

 fail:
        free(env);
        return (rv);
}

static int
find_device_node(struct error *err, const char *root, const char *dev, struct nvc_device_node *node)
{
        char path[PATH_MAX];
        struct stat s;

        if (path_resolve_full(err, path, root, dev) < 0)
                return (-1);
        if (xstat(err, path, &s) == 0) {
                *node = (struct nvc_device_node){(char *)dev, s.st_rdev};
                return (true);
        }
        if (err->code == ENOENT) {
                log_warnf("missing device %s", dev);
                return (false);
        }
        return (-1);
}

static int
find_ipc_path(struct error *err, const char *root, const char *ipc, char **buf)
{
        char path[PATH_MAX];
        int ret;

        if (path_resolve(err, path, root, ipc) < 0)
                return (-1);
        if ((ret = file_exists_at(err, root, path)) < 0)
                return (-1);
        if (ret) {
                log_infof("listing ipc %s", path);
                if ((*buf = xstrdup(err, path)) == NULL)
                        return (-1);
        } else {
                log_warnf("missing ipc %s", ipc);
        }
        return (0);
}

static int
lookup_libraries(struct error *err, struct nvc_driver_info *info, const char *root, int32_t flags, const char *ldcache)
{
        const char *libs[MAX_LIBS];
        const char **ptr = libs;

        ptr = array_append(ptr, utility_libs, nitems(utility_libs));
        ptr = array_append(ptr, compute_libs, nitems(compute_libs));
        ptr = array_append(ptr, video_libs, nitems(video_libs));
        ptr = array_append(ptr, graphics_libs, nitems(graphics_libs));
        if (flags & OPT_NO_GLVND)
                ptr = array_append(ptr, graphics_libs_compat, nitems(graphics_libs_compat));
        else
                ptr = array_append(ptr, graphics_libs_glvnd, nitems(graphics_libs_glvnd));

        if (find_library_paths(err, info, root, ldcache, libs, (size_t)(ptr - libs)) < 0)
                return (-1);

        for (size_t i = 0; info->libs != NULL && i < info->nlibs; ++i) {
                if (info->libs[i] == NULL)
                        log_warnf("missing library %s", libs[i]);
        }
        for (size_t i = 0; info->libs32 != NULL && i < info->nlibs32; ++i) {
                if (info->libs32[i] == NULL)
                        log_warnf("missing compat32 library %s", libs[i]);
        }
        array_pack(info->libs, &info->nlibs);
        array_pack(info->libs32, &info->nlibs32);
        return (0);
}

static int
lookup_binaries(struct error *err, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        const char *bins[MAX_BINS];
        const char **ptr = bins;

        ptr = array_append(ptr, utility_bins, nitems(utility_bins));
        if (!(flags & OPT_NO_MPS))
                ptr = array_append(ptr, compute_bins, nitems(compute_bins));

        if (find_binary_paths(err, info, root, bins, (size_t)(ptr - bins)) < 0)
                return (-1);

        for (size_t i = 0; info->bins != NULL && i < info->nbins; ++i) {
                if (info->bins[i] == NULL)
                        log_warnf("missing binary %s", bins[i]);
        }
        array_pack(info->bins, &info->nbins);
        return (0);
}

static int
lookup_devices(struct error *err, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        struct nvc_device_node uvm, uvm_tools, modeset, *node;
        int has_uvm = 0;
        int has_uvm_tools = 0;
        int has_modeset = 0;

        if (!(flags & OPT_NO_UVM)) {
                if ((has_uvm = find_device_node(err, root, NV_UVM_DEVICE_PATH, &uvm)) < 0)
                        return (-1);
                if ((has_uvm_tools = find_device_node(err, root, NV_UVM_TOOLS_DEVICE_PATH, &uvm_tools)) < 0)
                        return (-1);
        }
        if (!(flags & OPT_NO_MODESET)) {
                modeset.path = (char *)NV_MODESET_DEVICE_PATH;
                modeset.id = makedev(NV_DEVICE_MAJOR, NV_MODESET_DEVICE_MINOR);
                has_modeset = 1;
        }

        info->ndevs = (size_t)(1 + has_uvm + has_uvm_tools + has_modeset);
        info->devs = node = xcalloc(err, info->ndevs, sizeof(*info->devs));
        if (info->devs == NULL)
                return (-1);

        node->path = (char *)NV_CTL_DEVICE_PATH;
        node->id = makedev(NV_DEVICE_MAJOR, NV_CTL_DEVICE_MINOR);
        if (has_uvm)
                *(++node) = uvm;
        if (has_uvm_tools)
                *(++node) = uvm_tools;
        if (has_modeset)
                *(++node) = modeset;

        for (size_t i = 0; i < info->ndevs; ++i)
                log_infof("listing device %s", info->devs[i].path);
        return (0);
}

static int
lookup_ipcs(struct error *err, struct nvc_driver_info *info, const char *root, int32_t flags)
{
        char **ptr;
        const char *mps;

        info->nipcs = 2;
        info->ipcs = ptr = array_new(err, info->nipcs);
        if (info->ipcs == NULL)
                return (-1);

        if (!(flags & OPT_NO_PERSISTENCED)) {
                if (find_ipc_path(err, root, NV_PERSISTENCED_SOCKET, ptr++) < 0)
                        return (-1);
        }
        if (!(flags & OPT_NO_MPS)) {
                if ((mps = secure_getenv("CUDA_MPS_PIPE_DIRECTORY")) == NULL)
                        mps = NV_MPS_PIPE_DIR;
                if (find_ipc_path(err, root, mps, ptr++) < 0)
                        return (-1);
        }
        array_pack(info->ipcs, &info->nipcs);
        return (0);
}

bool
match_binary_flags(const char *bin, int32_t flags)
{
        if ((flags & OPT_UTILITY_BINS) && str_array_match_prefix(bin, utility_bins, nitems(utility_bins)))
                return (true);
        if ((flags & OPT_COMPUTE_BINS) && str_array_match_prefix(bin, compute_bins, nitems(compute_bins)))
                return (true);
        return (false);
}

bool
match_library_flags(const char *lib, int32_t flags)
{
        if ((flags & OPT_UTILITY_LIBS) && str_array_match_prefix(lib, utility_libs, nitems(utility_libs)))
                return (true);
        if ((flags & OPT_COMPUTE_LIBS) && str_array_match_prefix(lib, compute_libs, nitems(compute_libs)))
                return (true);
        if ((flags & OPT_VIDEO_LIBS) && str_array_match_prefix(lib, video_libs, nitems(video_libs)))
                return (true);
        if ((flags & OPT_GRAPHICS_LIBS) && (str_array_match_prefix(lib, graphics_libs, nitems(graphics_libs)) ||
            str_array_match_prefix(lib, graphics_libs_glvnd, nitems(graphics_libs_glvnd)) ||
            str_array_match_prefix(lib, graphics_libs_compat, nitems(graphics_libs_compat))))
                return (true);
        return (false);
}

struct nvc_driver_info *
nvc_driver_info_new(struct nvc_context *ctx, const char *opts)
{
        struct nvc_driver_info *info;
        int32_t flags;

        if (validate_context(ctx) < 0)
                return (NULL);
        if (opts == NULL)
                opts = default_driver_opts;
        if ((flags = options_parse(&ctx->err, opts, driver_opts, nitems(driver_opts))) < 0)
                return (NULL);

        log_infof("requesting driver information with '%s'", opts);
        if ((info = xcalloc(&ctx->err, 1, sizeof(*info))) == NULL)
                return (NULL);

        if (driver_get_rm_version(&ctx->drv, &info->nvrm_version) < 0)
                goto fail;
        if (driver_get_cuda_version(&ctx->drv, &info->cuda_version) < 0)
                goto fail;
        if (lookup_libraries(&ctx->err, info, ctx->cfg.root, flags, ctx->cfg.ldcache) < 0)
                goto fail;
        if (lookup_binaries(&ctx->err, info, ctx->cfg.root, flags) < 0)
                goto fail;
        if (lookup_devices(&ctx->err, info, ctx->cfg.root, flags) < 0)
                goto fail;
        if (lookup_ipcs(&ctx->err, info, ctx->cfg.root, flags) < 0)
                goto fail;
        return (info);

 fail:
        nvc_driver_info_free(info);
        return (NULL);
}

void
nvc_driver_info_free(struct nvc_driver_info *info)
{
        if (info == NULL)
                return;
        free(info->nvrm_version);
        free(info->cuda_version);
        array_free(info->bins, info->nbins);
        array_free(info->libs, info->nlibs);
        array_free(info->libs32, info->nlibs32);
        array_free(info->ipcs, info->nipcs);
        free(info->devs);
        free(info);
}

struct nvc_device_info *
nvc_device_info_new(struct nvc_context *ctx, const char *opts)
{
        struct nvc_device_info *info;
        struct nvc_device *gpu;
        unsigned int n, minor;
        struct driver_device *dev;
        /*int32_t flags;*/

        if (validate_context(ctx) < 0)
                return (NULL);
        if (opts == NULL)
                opts = default_device_opts;
        /*
        if ((flags = options_parse(&ctx->err, opts, device_opts, nitems(device_opts))) < 0)
                return (NULL);
        */

        log_infof("requesting device information with '%s'", opts);
        if ((info = xcalloc(&ctx->err, 1, sizeof(*info))) == NULL)
                return (NULL);

        if (driver_get_device_count(&ctx->drv, &n) < 0)
                goto fail;
        info->ngpus = n;
        info->gpus = gpu = xcalloc(&ctx->err, info->ngpus, sizeof(*info->gpus));
        if (info->gpus == NULL)
                goto fail;

        for (unsigned int i = 0; i < n; ++i, ++gpu) {
                if (driver_get_device(&ctx->drv, i, &dev) < 0)
                        goto fail;
                if (driver_get_device_model(&ctx->drv, dev, &gpu->model) < 0)
                        goto fail;
                if (driver_get_device_uuid(&ctx->drv, dev, &gpu->uuid) < 0)
                        goto fail;
                if (driver_get_device_busid(&ctx->drv, dev, &gpu->busid) < 0)
                        goto fail;
                if (driver_get_device_arch(&ctx->drv, dev, &gpu->arch) < 0)
                        goto fail;
                if (driver_get_device_brand(&ctx->drv, dev, &gpu->brand) < 0)
                        goto fail;
                if (driver_get_device_minor(&ctx->drv, dev, &minor) < 0)
                        goto fail;
                if (xasprintf(&ctx->err, &gpu->node.path, NV_DEVICE_PATH, minor) < 0)
                        goto fail;
                gpu->node.id = makedev(NV_DEVICE_MAJOR, minor);

                log_infof("listing device %s (%s at %s)", gpu->node.path, gpu->uuid, gpu->busid);
        }
        return (info);

 fail:
        nvc_device_info_free(info);
        return (NULL);
}

void
nvc_device_info_free(struct nvc_device_info *info)
{
        if (info == NULL)
                return;
        for (size_t i = 0; info->gpus != NULL && i < info->ngpus; ++i) {
                free(info->gpus[i].model);
                free(info->gpus[i].uuid);
                free(info->gpus[i].busid);
                free(info->gpus[i].arch);
                free(info->gpus[i].brand);
                free(info->gpus[i].node.path);
        }
        free(info->gpus);
        free(info);
}
