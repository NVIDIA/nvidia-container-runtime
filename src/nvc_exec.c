/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/securebits.h>

#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>

#if !defined(PR_CAP_AMBIENT) || !defined(PR_CAP_AMBIENT_RAISE)
# define PR_CAP_AMBIENT       47
# define PR_CAP_AMBIENT_RAISE 2
#endif

#include <grp.h>
#include <sched.h>
#ifdef WITH_SECCOMP
#include <seccomp.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "nvc_internal.h"

#include "error.h"
#include "utils.h"
#include "xfuncs.h"

static pid_t create_process(struct error *);
static int   change_rootfs(struct error *, const char *, bool *);
static int   drop_capabilities(struct error *);
static int   drop_privileges(struct error *, uid_t, gid_t, bool);
static int   limit_resources(struct error *);
static int   limit_syscalls(struct error *);

static pid_t
create_process(struct error *err)
{
        pid_t child;
        int fd[2] = {-1, -1};
        int null = -1;
        int rv = -1;

        if ((log_active() && pipe(fd) < 0) || (child = fork()) < 0) {
                error_set(err, "process creation failed");
                xclose(fd[0]);
                xclose(fd[1]);
                return (-1);
        }

        if (child == 0) {
                if ((null = xopen(err, "/dev/null", O_RDWR)) < 0)
                        goto fail;
                if (dup2(null, STDIN_FILENO) < 0 ||
                    dup2(log_active() ? fd[1] : null, STDOUT_FILENO) < 0 ||
                    dup2(log_active() ? fd[1] : null, STDERR_FILENO) < 0) {
                        error_set(err, "file duplication failed");
                        goto fail;
                }
        } else {
                if (log_pipe_output(err, fd) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        if (rv < 0) {
                log_err("could not capture process output: %s", err->msg);
                error_reset(err);
        }
        xclose(fd[0]);
        xclose(fd[1]);
        xclose(null);
        return (child);
}

static int
change_rootfs(struct error *err, const char *rootfs, bool *drop_groups)
{
        int rv = -1;
        int oldroot = -1;
        int newroot = -1;
        FILE *fs;
        char buf[8] = {0};
        const char *mounts[] = {"/proc", "/sys", "/dev"};

        error_reset(err);

        /* Create a new mount namespace with private propagation. */
        if (unshare(CLONE_NEWNS) < 0)
                goto fail;
        if (xmount(err, NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) < 0)
                goto fail;
        if (xmount(err, rootfs, rootfs, NULL, MS_BIND|MS_REC, NULL) < 0)
                goto fail;

        /* Pivot to the new rootfs and unmount the previous one. */
        if ((oldroot = xopen(err, "/", O_RDONLY|O_DIRECTORY)) < 0)
                goto fail;
        if ((newroot = xopen(err, rootfs, O_RDONLY|O_DIRECTORY)) < 0)
                goto fail;
        if (fchdir(newroot) < 0)
                goto fail;
        if (syscall(SYS_pivot_root, ".", ".") < 0)
                goto fail;
        if (fchdir(oldroot) < 0)
                goto fail;
        if (umount2(".", MNT_DETACH) < 0)
                goto fail;
        if (fchdir(newroot) < 0)
                goto fail;
        if (chroot(".") < 0)
                goto fail;

        if ((fs = fopen(PROC_SETGROUPS_PATH(PROC_SELF), "r")) != NULL) {
                (void)fgets(buf, sizeof(buf), fs);
                fclose(fs);
        }
        *drop_groups = strpcmp(buf, "deny");

        /* Hide sensitive mountpoints. */
        for (size_t i = 0; i < nitems(mounts); ++i) {
                if (xmount(err, NULL, mounts[i], "tmpfs", MS_RDONLY, NULL) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        if (rv < 0 && err->code == 0)
                error_set(err, "process confinement failed");
        xclose(oldroot);
        xclose(newroot);
        return (rv);
}

static int
drop_capabilities(struct error *err)
{
        FILE *fs;
        cap_t caps = NULL;
        cap_value_t last_cap = 63;
        cap_value_t cap = CAP_DAC_OVERRIDE;
        int rv = -1;

        if ((fs = fopen(PROC_LAST_CAP_PATH, "r")) != NULL) {
                int v;
                if (fscanf(fs, "%d", &v) == 1) {
                        if ((cap_value_t)v >= 0 && (cap_value_t)v <= last_cap)
                                last_cap = (cap_value_t)v;
                }
                fclose(fs);
        }

        /*
         * Drop all the inheritable capabilities (and the ambient capabilities consequently).
         * We need to keep CAP_DAC_OVERRIDE because some distributions rely on it
         * (e.g. https://bugzilla.redhat.com/show_bug.cgi?id=517575)
         */
        if ((caps = cap_get_proc()) == NULL)
                goto fail;
        if (cap_clear_flag(caps, CAP_INHERITABLE) < 0)
                goto fail;
        if (cap_set_flag(caps, CAP_INHERITABLE, 1, &cap, CAP_SET) < 0)
                goto fail;
        if (cap_set_proc(caps) < 0)
                goto fail;
        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) < 0 && errno != EINVAL)
                goto fail;

        /* Drop all the bounding capabilities */
        for (cap_value_t c = 0; c <= last_cap; ++c) {
                if (CAP_IS_SUPPORTED(c) && cap_drop_bound(c) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        if (rv < 0)
                error_set(err, "capabilities relinquishment failed");
        cap_free(caps);
        return (rv);
}

static int
drop_privileges(struct error *err, uid_t uid, gid_t gid, bool drop_groups)
{
        if (prctl(PR_SET_SECUREBITS, SECBIT_NO_SETUID_FIXUP, 0, 0, 0) < 0)
                goto fail;

        if (drop_groups && setgroups(1, &gid) < 0)
                goto fail;
        if (setregid(gid, gid) < 0)
                goto fail;
        if (setreuid(uid, uid) < 0)
                goto fail;
        if (getegid() != gid || geteuid() != uid) {
                errno = EPERM;
                goto fail;
        }

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0)
                goto fail;
        if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) < 0)
                goto fail;
        return (0);

 fail:
        error_set(err, "privileges relinquishment failed");
        return (-1);
}

static int
limit_resources(struct error *err)
{
        struct rlimit limit;

        limit = (struct rlimit){10, 10};
        if (setrlimit(RLIMIT_CPU, &limit) < 0)
                goto fail;
        limit = (struct rlimit){512*1024*1024, 512*1024*1024};
        if (setrlimit(RLIMIT_AS, &limit) < 0)
                goto fail;
        limit = (struct rlimit){64, 64};
        if (setrlimit(RLIMIT_NOFILE, &limit) < 0)
                goto fail;
        limit = (struct rlimit){1024*1024, 1024*1024};
        if (setrlimit(RLIMIT_FSIZE, &limit) < 0)
                goto fail;
        return (0);

 fail:
        error_set(err, "resource limiting failed");
        return (-1);
}

#ifdef WITH_SECCOMP
static int
limit_syscalls(struct error *err)
{
        scmp_filter_ctx ctx;
        int syscalls[] = {
                SCMP_SYS(access),
                SCMP_SYS(arch_prctl),
                SCMP_SYS(brk),
                SCMP_SYS(chmod),
                SCMP_SYS(close),
                SCMP_SYS(execve),
                SCMP_SYS(exit),
                SCMP_SYS(fcntl),
                SCMP_SYS(fstat),
                SCMP_SYS(fsync),
                SCMP_SYS(getdents),
                SCMP_SYS(gettid),
                SCMP_SYS(getpid),
                SCMP_SYS(gettimeofday),
                SCMP_SYS(lseek),
                SCMP_SYS(lstat),
                SCMP_SYS(mmap),
                SCMP_SYS(mprotect),
                SCMP_SYS(munmap),
                SCMP_SYS(newfstatat),
                SCMP_SYS(open),
                SCMP_SYS(openat),
                SCMP_SYS(read),
                SCMP_SYS(readlink),
                SCMP_SYS(rename),
                SCMP_SYS(stat),
                SCMP_SYS(symlink),
                SCMP_SYS(uname),
                SCMP_SYS(unlink),
                SCMP_SYS(write),
        };
        int rv = -1;

        if ((ctx = seccomp_init(SCMP_ACT_ERRNO(EPERM))) == NULL)
                goto fail;
        for (size_t i = 0; i < nitems(syscalls); ++i) {
                if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscalls[i], 0) < 0)
                        goto fail;
        }
        if (seccomp_load(ctx) < 0)
                goto fail;
        rv = 0;

 fail:
        if (rv < 0)
                error_setx(err, "syscall limiting failed");
        seccomp_release(ctx);
        return (rv);
}
#else
static int
limit_syscalls(maybe_unused struct error *err)
{
        log_warn("secure computing is disabled, all syscalls are allowed");
        return (0);
}
#endif /* WITH_SECCOMP */

int
nvc_ldcache_update(struct nvc_context *ctx, const struct nvc_container *cnt)
{
        pid_t child;
        int status;
        bool drop_groups = true;

        if (validate_context(ctx) < 0)
                return (-1);
        if (validate_args(ctx, cnt != NULL) < 0)
                return (-1);

        log_info("executing %s at %s", cnt->cfg.ldconfig, cnt->cfg.rootfs);
        if ((child = create_process(&ctx->err)) < 0)
                return (-1);
        if (child == 0) {
                prctl(PR_SET_NAME, (unsigned long)"nvc:[ldconfig]", 0, 0, 0);

                if (nsenter(&ctx->err, cnt->mnt_ns, CLONE_NEWNS) < 0)
                        goto fail;
                if (change_rootfs(&ctx->err, cnt->cfg.rootfs, &drop_groups) < 0)
                        goto fail;
                if (limit_resources(&ctx->err) < 0)
                        goto fail;
                if (drop_capabilities(&ctx->err) < 0)
                        goto fail;
                if (drop_privileges(&ctx->err, cnt->uid, cnt->gid, drop_groups) < 0)
                        goto fail;
                if (limit_syscalls(&ctx->err) < 0)
                        goto fail;

                execle(cnt->cfg.ldconfig, cnt->cfg.ldconfig, cnt->cfg.libs_dir, cnt->cfg.libs32_dir,
                    (char *)NULL, (char * const []){NULL});
                error_set(&ctx->err, "process execution failed");
         fail:
                log_err("could not start %s: %s", cnt->cfg.ldconfig, ctx->err.msg);
                (ctx->err.code == ENOENT) ? _exit(EXIT_SUCCESS) : _exit(EXIT_FAILURE);
        }
        if (waitpid(child, &status, 0) < 0) {
                error_set(&ctx->err, "process reaping failed");
                return (-1);
        }
        if (WIFSIGNALED(status)) {
                error_setx(&ctx->err, "process %s terminated with signal %d", cnt->cfg.ldconfig, WTERMSIG(status));
                return (-1);
        }
        if (WIFEXITED(status) && (status = WEXITSTATUS(status)) != 0) {
                error_setx(&ctx->err, "process %s failed with error code: %d", cnt->cfg.ldconfig, status);
                return (-1);
        }
        return (0);
}
