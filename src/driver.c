/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include "cuda.h"
#include "nvml.h"

#pragma GCC diagnostic push
#include "driver_rpc.h"
#pragma GCC diagnostic pop

#include "driver.h"
#include "error.h"
#include "utils.h"
#include "xfuncs.h"

#define MAX_DEVICES     64
#define REAP_TIMEOUT_MS 10

static int reset_cuda_environment(struct error *);
static int setup_rpc_client(struct driver *);
static noreturn void setup_rpc_service(struct driver *, const char *, uid_t, gid_t, pid_t);
static int reap_process(struct error *, pid_t, int, bool);

static struct driver_device {
        nvmlDevice_t nvml;
        CUdevice cuda;
} device_handles[MAX_DEVICES];

#define call_nvml(ctx, sym, ...) __extension__ ({                                                      \
        union {void *ptr; __typeof__(&sym) fn;} u_;                                                    \
        nvmlReturn_t r_;                                                                               \
                                                                                                       \
        dlerror();                                                                                     \
        u_.ptr = dlsym((ctx)->nvml_dl, #sym);                                                          \
        r_ = (dlerror() == NULL) ? (*u_.fn)(__VA_ARGS__) : NVML_ERROR_FUNCTION_NOT_FOUND;              \
        if (r_ != NVML_SUCCESS)                                                                        \
                error_set_nvml((ctx)->err, (ctx)->nvml_dl, r_, "nvml error");                          \
        (r_ == NVML_SUCCESS) ? 0 : -1;                                                                 \
})

#define call_cuda(ctx, sym, ...) __extension__ ({                                                      \
        union {void *ptr; __typeof__(&sym) fn;} u_;                                                    \
        CUresult r_;                                                                                   \
                                                                                                       \
        dlerror();                                                                                     \
        u_.ptr = dlsym((ctx)->cuda_dl, #sym);                                                          \
        r_ = (dlerror() == NULL) ? (*u_.fn)(__VA_ARGS__) : CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND;  \
        if (r_ != CUDA_SUCCESS)                                                                        \
                error_set_cuda((ctx)->err, (ctx)->cuda_dl, r_, "cuda error");                          \
        (r_ == CUDA_SUCCESS) ? 0 : -1;                                                                 \
})

#define call_rpc(ctx, res, func, ...) __extension__ ({                                                 \
        enum clnt_stat r_;                                                                             \
        struct sigaction osa_, sa_ = {.sa_handler = SIG_IGN};                                          \
                                                                                                       \
        static_assert(sizeof(ptr_t) >= sizeof(intptr_t), "incompatible types");                        \
        sigaction(SIGPIPE, &sa_, &osa_);                                                               \
        if ((r_ = func((ptr_t)ctx, ##__VA_ARGS__, res, (ctx)->rpc_clt)) != RPC_SUCCESS)                \
                error_set_rpc((ctx)->err, r_, "driver error");                                         \
        else if ((res)->errcode != 0)                                                                  \
                error_from_xdr((ctx)->err, res);                                                       \
        sigaction(SIGPIPE, &osa_, NULL);                                                               \
        (r_ == RPC_SUCCESS && (res)->errcode == 0) ? 0 : -1;                                           \
})

static int
reset_cuda_environment(struct error *err)
{
        const struct { const char *name, *value; } env[] = {
                {"CUDA_DISABLE_UNIFIED_MEMORY", "1"},
                {"CUDA_CACHE_DISABLE", "1"},
                {"CUDA_DEVICE_ORDER", "PCI_BUS_ID"},
                {"CUDA_VISIBLE_DEVICES", NULL},
                {"CUDA_MPS_PIPE_DIRECTORY", "/dev/null"},
        };
        int ret;

        for (size_t i = 0; i < nitems(env); ++i) {
                ret = (env[i].value == NULL) ? unsetenv(env[i].name) :
                    setenv(env[i].name, env[i].value, 1);
                if (ret < 0) {
                        error_set(err, "environment reset failed");
                        return (-1);
                }
        }
        return (0);
}

static int
setup_rpc_client(struct driver *ctx)
{
        struct sockaddr_un addr;
        socklen_t addrlen;
        struct timeval timeout = {10, 0};

        xclose(ctx->fd[SOCK_SVC]);

        addrlen = sizeof(addr);
        if (getpeername(ctx->fd[SOCK_CLT], (struct sockaddr *)&addr, &addrlen) < 0) {
                error_set(ctx->err, "address resolution failed");
                return (-1);
        }
        if ((ctx->rpc_clt = clntunix_create(&addr, DRIVER_PROGRAM, DRIVER_VERSION, &ctx->fd[SOCK_CLT], 0, 0)) == NULL) {
                error_setx(ctx->err, "%s", clnt_spcreateerror("driver client creation failed"));
                return (-1);
        }
        clnt_control(ctx->rpc_clt, CLSET_TIMEOUT, (char *)&timeout);
        return (0);
}

static void
setup_rpc_service(struct driver *ctx, const char *root, uid_t uid, gid_t gid, pid_t ppid)
{
        int rv = EXIT_FAILURE;

        log_info("starting driver service");
        prctl(PR_SET_NAME, (unsigned long)"nvc:[driver]", 0, 0, 0);

        xclose(ctx->fd[SOCK_CLT]);

        if (!str_equal(root, "/")) {
                /* Preload glibc libraries to avoid symbols mismatch after changing root. */
                if (xdlopen(ctx->err, "libm.so.6", RTLD_NOW) == NULL)
                        goto fail;
                if (xdlopen(ctx->err, "librt.so.1", RTLD_NOW) == NULL)
                        goto fail;
                if (xdlopen(ctx->err, "libpthread.so.0", RTLD_NOW) == NULL)
                        goto fail;

                if (chroot(root) < 0 || chdir("/") < 0) {
                        error_set(ctx->err, "change root failed");
                        goto fail;
                }
        }

        /*
         * Drop privileges and capabilities for security reasons.
         *
         * We might be inside a user namespace with full capabilities, this should also help prevent CUDA and NVML
         * from potentially adjusting the host device nodes based on the (wrong) driver registry parameters.
         *
         * If we are not changing group, then keep our supplementary groups as well.
         * This is arguable but allows us to support unprivileged processes (i.e. without CAP_SETGID) and user namespaces.
         */
        if (perm_drop_privileges(ctx->err, uid, gid, (getegid() != gid)) < 0)
                goto fail;
        if (perm_set_capabilities(ctx->err, CAP_PERMITTED, NULL, 0) < 0)
                goto fail;
        if (reset_cuda_environment(ctx->err) < 0)
                goto fail;

        /*
         * Set PDEATHSIG in case our parent terminates unexpectedly.
         * We need to do it late since the kernel resets it on credential change.
         */
        if (prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0) < 0) {
                error_set(ctx->err, "process initialization failed");
                goto fail;
        }
        if (getppid() != ppid)
                kill(getpid(), SIGTERM);

        if ((ctx->cuda_dl = xdlopen(ctx->err, SONAME_LIBCUDA, RTLD_NOW)) == NULL)
                goto fail;
        if ((ctx->nvml_dl = xdlopen(ctx->err, SONAME_LIBNVML, RTLD_NOW)) == NULL)
                goto fail;

        if ((ctx->rpc_svc = svcunixfd_create(ctx->fd[SOCK_SVC], 0, 0)) == NULL ||
            !svc_register(ctx->rpc_svc, DRIVER_PROGRAM, DRIVER_VERSION, driver_program_1, 0)) {
                error_setx(ctx->err, "program registration failed");
                goto fail;
        }
        svc_run();

        log_info("terminating driver service");
        rv = EXIT_SUCCESS;

 fail:
        if (rv != EXIT_SUCCESS)
                log_errf("could not start driver service: %s", ctx->err->msg);
        xdlclose(NULL, ctx->cuda_dl);
        xdlclose(NULL, ctx->nvml_dl);
        if (ctx->rpc_svc != NULL)
                svc_destroy(ctx->rpc_svc);
        _exit(rv);
}

static int
reap_process(struct error *err, pid_t pid, int fd, bool force)
{
        int ret = 0;
        int status;
        struct pollfd fds = {.fd = fd, .events = POLLRDHUP};

        if (waitpid(pid, &status, WNOHANG) <= 0) {
                if (force)
                        kill(pid, SIGTERM);

                switch ((ret = poll(&fds, 1, REAP_TIMEOUT_MS))) {
                case -1:
                        break;
                case 0:
                        log_warn("terminating driver service (forced)");
                        ret = kill(pid, SIGKILL);
                        /* Fallthrough */
                default:
                        if (ret >= 0)
                                ret = waitpid(pid, &status, 0);
                }
        }
        if (ret < 0)
                error_set(err, "process reaping failed (pid %"PRId32")", (int32_t)pid);
        else
                log_infof("driver service terminated %s%.0d",
                    WIFSIGNALED(status) ? "with signal " : "successfully",
                    WIFSIGNALED(status) ? WTERMSIG(status) : 0);
        return (ret);
}

int
driver_program_1_freeresult(maybe_unused SVCXPRT *svc, xdrproc_t xdr_result, caddr_t res)
{
        xdr_free(xdr_result, res);
        return (1);
}

int
driver_init(struct driver *ctx, struct error *err, const char *root, uid_t uid, gid_t gid)
{
        int ret;
        pid_t pid;
        struct driver_init_res res = {0};

        *ctx = (struct driver){err, NULL, NULL, {-1, -1}, -1, NULL, NULL};

        pid = getpid();
        if (socketpair(PF_LOCAL, SOCK_STREAM|SOCK_CLOEXEC, 0, ctx->fd) < 0 || (ctx->pid = fork()) < 0) {
                error_set(err, "process creation failed");
                goto fail;
        }
        if (ctx->pid == 0)
                setup_rpc_service(ctx, root, uid, gid, pid);
        if (setup_rpc_client(ctx) < 0)
                goto fail;

        ret = call_rpc(ctx, &res, driver_init_1);
        xdr_free((xdrproc_t)xdr_driver_init_res, (caddr_t)&res);
        if (ret < 0)
                goto fail;

        return (0);

 fail:
        if (ctx->pid > 0 && reap_process(NULL, ctx->pid, ctx->fd[SOCK_CLT], true) < 0)
                log_warnf("could not terminate driver service (pid %"PRId32")", (int32_t)ctx->pid);
        if (ctx->rpc_clt != NULL)
                clnt_destroy(ctx->rpc_clt);

        xclose(ctx->fd[SOCK_CLT]);
        xclose(ctx->fd[SOCK_SVC]);
        return (-1);
}

bool_t
driver_init_1_svc(ptr_t ctxptr, driver_init_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;

        memset(res, 0, sizeof(*res));
        if (call_cuda(ctx, cuInit, 0) < 0)
                goto fail;
        if (call_nvml(ctx, nvmlInit_v2) < 0)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_shutdown(struct driver *ctx)
{
        int ret;
        struct driver_shutdown_res res = {0};

        ret = call_rpc(ctx, &res, driver_shutdown_1);
        xdr_free((xdrproc_t)xdr_driver_shutdown_res, (caddr_t)&res);
        if (ret < 0)
                log_warnf("could not terminate driver service: %s", ctx->err->msg);

        if (reap_process(ctx->err, ctx->pid, ctx->fd[SOCK_CLT], (ret < 0)) < 0)
                return (-1);
        clnt_destroy(ctx->rpc_clt);

        xclose(ctx->fd[SOCK_CLT]);
        xclose(ctx->fd[SOCK_SVC]);
        *ctx = (struct driver){NULL, NULL, NULL, {-1, -1}, -1, NULL, NULL};
        return (0);
}

bool_t
driver_shutdown_1_svc(ptr_t ctxptr, driver_shutdown_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlShutdown) < 0)
                goto fail;
        svc_exit();
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_rm_version(struct driver *ctx, char **version)
{
        struct driver_get_rm_version_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_rm_version_1) < 0)
                goto fail;
        if ((*version = xstrdup(ctx->err, res.driver_get_rm_version_res_u.vers)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_rm_version_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_rm_version_1_svc(ptr_t ctxptr, driver_get_rm_version_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        char buf[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlSystemGetDriverVersion, buf, sizeof(buf)) < 0)
                goto fail;
        if ((res->driver_get_rm_version_res_u.vers = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_cuda_version(struct driver *ctx, char **version)
{
        struct driver_get_cuda_version_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_cuda_version_1) < 0)
                goto fail;
        if (xasprintf(ctx->err, version, "%u.%u", res.driver_get_cuda_version_res_u.vers.major,
            res.driver_get_cuda_version_res_u.vers.minor) < 0)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_cuda_version_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_cuda_version_1_svc(ptr_t ctxptr, driver_get_cuda_version_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        int version;

        memset(res, 0, sizeof(*res));
        if (call_cuda(ctx, cuDriverGetVersion, &version) < 0)
                goto fail;
        res->driver_get_cuda_version_res_u.vers.major = (unsigned int)version / 1000;
        res->driver_get_cuda_version_res_u.vers.minor = (unsigned int)version % 100 / 10;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_count(struct driver *ctx, unsigned int *count)
{
        struct driver_get_device_count_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_count_1) < 0)
                goto fail;
        *count = res.driver_get_device_count_res_u.count;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_count_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_count_1_svc(ptr_t ctxptr, driver_get_device_count_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        unsigned int count;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetCount_v2, &count) < 0)
                goto fail;
        res->driver_get_device_count_res_u.count = count;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device(struct driver *ctx, unsigned int idx, struct driver_device **dev)
{
        struct driver_get_device_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_1, idx) < 0)
                goto fail;
        *dev = (struct driver_device *)res.driver_get_device_res_u.dev;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_1_svc(ptr_t ctxptr, u_int idx, driver_get_device_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        int domainid, deviceid, busid;
        char buf[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE + 1];

        memset(res, 0, sizeof(*res));
        if (idx >= MAX_DEVICES) {
                error_setx(ctx->err, "too many devices");
                goto fail;
        }
        if (call_cuda(ctx, cuDeviceGet, &device_handles[idx].cuda, (int)idx) < 0)
                goto fail;
        if (call_cuda(ctx, cuDeviceGetAttribute, &domainid, CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, device_handles[idx].cuda) < 0)
                goto fail;
        if (call_cuda(ctx, cuDeviceGetAttribute, &busid, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, device_handles[idx].cuda) < 0)
                goto fail;
        if (call_cuda(ctx, cuDeviceGetAttribute, &deviceid, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, device_handles[idx].cuda) < 0)
                goto fail;
        snprintf(buf, sizeof(buf), "%08x:%02x:%02x.0", domainid, busid, deviceid);
        if (call_nvml(ctx, nvmlDeviceGetHandleByPciBusId_v2, buf, &device_handles[idx].nvml) < 0)
                goto fail;

        res->driver_get_device_res_u.dev = (ptr_t)&device_handles[idx];
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_minor(struct driver *ctx, struct driver_device *dev, unsigned int *minor)
{
        struct driver_get_device_minor_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_minor_1, (ptr_t)dev) < 0)
                goto fail;
        *minor = res.driver_get_device_minor_res_u.minor;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_minor_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_minor_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_minor_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        unsigned int minor;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetMinorNumber, handle->nvml, &minor) < 0)
                goto fail;
        res->driver_get_device_minor_res_u.minor = minor;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_busid(struct driver *ctx, struct driver_device *dev, char **busid)
{
        struct driver_get_device_busid_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_busid_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*busid = xstrdup(ctx->err, res.driver_get_device_busid_res_u.busid)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_busid_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_busid_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_busid_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        int domainid, deviceid, busid;

        memset(res, 0, sizeof(*res));
        if (call_cuda(ctx, cuDeviceGetAttribute, &domainid, CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, handle->cuda) < 0)
                goto fail;
        if (call_cuda(ctx, cuDeviceGetAttribute, &busid, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, handle->cuda) < 0)
                goto fail;
        if (call_cuda(ctx, cuDeviceGetAttribute, &deviceid, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, handle->cuda) < 0)
                goto fail;
        if (xasprintf(ctx->err, &res->driver_get_device_busid_res_u.busid, "%08x:%02x:%02x.0", domainid, busid, deviceid) < 0)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_uuid(struct driver *ctx, struct driver_device *dev, char **uuid)
{
        struct driver_get_device_uuid_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_uuid_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*uuid = xstrdup(ctx->err, res.driver_get_device_uuid_res_u.uuid)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_uuid_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_uuid_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_uuid_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        char buf[NVML_DEVICE_UUID_BUFFER_SIZE];

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetUUID, handle->nvml, buf, sizeof(buf)) < 0)
                goto fail;
        if ((res->driver_get_device_uuid_res_u.uuid = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_model(struct driver *ctx, struct driver_device *dev, char **model)
{
        struct driver_get_device_model_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_model_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*model = xstrdup(ctx->err, res.driver_get_device_model_res_u.model)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_model_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_model_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_model_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        char buf[NVML_DEVICE_NAME_BUFFER_SIZE];

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetName, handle->nvml, buf, sizeof(buf)) < 0)
                goto fail;
        if ((res->driver_get_device_model_res_u.model = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_brand(struct driver *ctx, struct driver_device *dev, char **brand)
{
        struct driver_get_device_brand_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_brand_1, (ptr_t)dev) < 0)
                goto fail;
        if ((*brand = xstrdup(ctx->err, res.driver_get_device_brand_res_u.brand)) == NULL)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_brand_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_brand_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_brand_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        nvmlBrandType_t brand;
        const char *buf;

        memset(res, 0, sizeof(*res));
        if (call_nvml(ctx, nvmlDeviceGetBrand, handle->nvml, &brand) < 0)
                goto fail;
        switch (brand) {
        case NVML_BRAND_QUADRO:
                buf = "Quadro";
                break;
        case NVML_BRAND_TESLA:
                buf = "Tesla";
                break;
        case NVML_BRAND_NVS:
                buf = "NVS";
                break;
        case NVML_BRAND_GRID:
                buf = "GRID";
                break;
        case NVML_BRAND_GEFORCE:
                buf = "GeForce";
                break;
        case NVML_BRAND_TITAN:
                buf = "TITAN";
                break;
        default:
                buf = "Unknown";
        }
        if ((res->driver_get_device_brand_res_u.brand = xstrdup(ctx->err, buf)) == NULL)
                goto fail;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}

int
driver_get_device_arch(struct driver *ctx, struct driver_device *dev, char **arch)
{
        struct driver_get_device_arch_res res = {0};
        int rv = -1;

        if (call_rpc(ctx, &res, driver_get_device_arch_1, (ptr_t)dev) < 0)
                goto fail;
        if (xasprintf(ctx->err, arch, "%u.%u", res.driver_get_device_arch_res_u.arch.major,
            res.driver_get_device_arch_res_u.arch.minor) < 0)
                goto fail;
        rv = 0;

 fail:
        xdr_free((xdrproc_t)xdr_driver_get_device_arch_res, (caddr_t)&res);
        return (rv);
}

bool_t
driver_get_device_arch_1_svc(ptr_t ctxptr, ptr_t dev, driver_get_device_arch_res *res, maybe_unused struct svc_req *req)
{
        struct driver *ctx = (struct driver *)ctxptr;
        struct driver_device *handle = (struct driver_device *)dev;
        int major, minor;

        memset(res, 0, sizeof(*res));
        if (call_cuda(ctx, cuDeviceGetAttribute, &major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, handle->cuda) < 0)
                goto fail;
        if (call_cuda(ctx, cuDeviceGetAttribute, &minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, handle->cuda) < 0)
                goto fail;
        res->driver_get_device_arch_res_u.arch.major = (unsigned int)major;
        res->driver_get_device_arch_res_u.arch.minor = (unsigned int)minor;
        return (true);

 fail:
        error_to_xdr(ctx->err, res);
        return (true);
}
