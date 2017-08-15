/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <sys/fsuid.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <grp.h>
#include <inttypes.h>
#include <libgen.h>
#undef basename /* Use the GNU version of basename. */
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "utils.h"
#include "xfuncs.h"

#if !defined(PR_CAP_AMBIENT) || !defined(PR_CAP_AMBIENT_RAISE) || !defined(PR_CAP_AMBIENT_CLEAR_ALL)
# define PR_CAP_AMBIENT           47
# define PR_CAP_AMBIENT_RAISE     2
# define PR_CAP_AMBIENT_CLEAR_ALL 4
#endif /* !defined(PR_CAP_AMBIENT) || !defined(PR_CAP_AMBIENT_RAISE) || !defined(PR_CAP_AMBIENT_CLEAR_ALL) */

static mode_t get_umask(void);
static int set_fsugid(uid_t, gid_t);
static int make_ancestors(char *, mode_t);
static int do_file_remove(const char *, const struct stat *, int, struct FTW *);
static int openrel(struct error *, int, const char *);

static FILE *logfile;

bool
log_active(void)
{
        return (logfile != NULL);
}

void
log_open(const char *path)
{
        if (path == NULL || log_active())
                return;
        logfile = fopen(path, "ae");
        assert(logfile != NULL);
        if (log_active()) {
                setbuf(logfile, NULL);
                fprintf(logfile, "\n-- WARNING, the following logs are for debugging purposes only --\n\n");
        }
}

void
log_close(void)
{
        if (!log_active())
                return;
        fclose(logfile);
        logfile = NULL;
}

void
log_write(char level, const char *file, unsigned long line, const char *fmt, ...)
{
        struct timeval tv = {0};
        struct tm *tm;
        char buf[16];
        va_list ap;

        if (!log_active())
                return;
        if (gettimeofday(&tv, NULL) < 0 || (tm = gmtime(&tv.tv_sec)) == NULL ||
            strftime(buf, sizeof(buf), "%m%d %T", tm) == 0)
                strcpy(buf, "0000 00:00:00");

        fprintf(logfile, "%c%s.%06ld %ld %s:%lu] ", level, buf, tv.tv_usec, (long)syscall(SYS_gettid), basename(file), line);
        va_start(ap, fmt);
        vfprintf(logfile, fmt, ap);
        va_end(ap);
        fputc('\n', logfile);
}

int
log_pipe_output(struct error *err, int fd[2])
{
        FILE *fs;
        ssize_t n;
        char *buf = NULL;
        size_t len = 0;
        int rv = 0;

        if (!log_active())
                return (0);
        if (fd[1] >= 0) {
                close(fd[1]);
                fd[1] = -1;
        }
        if ((fs = fdopen(fd[0], "r")) == NULL) {
                error_set(err, "open failed");
                return (-1);
        }
        while ((n = getline(&buf, &len, fs)) >= 0) {
                if (n > 0) {
                        buf[n - 1] = '\0';
                        log_warnf("%s", buf);
                }
        }
        if (ferror(fs)) {
                error_set(err, "read error");
                rv = -1;
        }
        free(buf);
        fclose(fs);
        return (rv);
}

void
strlower(char *str)
{
        for (char *p = str; *p != '\0'; ++p)
                *p = (char)tolower(*p);
}

int
strpcmp(const char *s1, const char *s2)
{
        return (strncmp(s1, s2, strlen(s2)));
}

int
strrcmp(const char *s1, const char *s2)
{
        size_t l1, l2;

        l1 = strlen(s1);
        l2 = strlen(s2);
        return ((l1 >= l2) ? strcmp(s1 + l1 - l2, s2) : -1);
}

bool
strempty(const char *str)
{
        return (str != NULL && *str == '\0');
}

bool
strmatch(const char *str, const char * const arr[], size_t size)
{
        for (size_t i = 0; i < size; ++i) {
                if (!strpcmp(str, arr[i]))
                        return (true);
        }
        return (false);
}

int
strjoin(struct error *err, char **s1, const char *s2, const char *sep)
{
        size_t size = 1;
        char *buf;

        if (*s1 != NULL && **s1 != '\0')
                size += strlen(*s1) + strlen(sep);
        size += strlen(s2);
        if ((buf = realloc(*s1, size)) == NULL) {
                error_set(err, "memory allocation failed");
                return (-1);
        }
        if (*s1 == NULL)
                *buf = '\0';
        if (*buf != '\0')
                strcat(buf, sep);
        strcat(buf, s2);
        *s1 = buf;
        return (0);
}

int
strtopid(struct error *err, const char *str, pid_t *pid)
{
        char *ptr;
        intmax_t n;

        n = strtoimax(str, &ptr, 10);
        if (ptr == str || *ptr != '\0') {
                errno = EINVAL;
                goto fail;
        }
        if (n == INTMAX_MIN || n == INTMAX_MAX || n != (pid_t)n) {
                errno = ERANGE;
                goto fail;
        }
        *pid = (pid_t)n;
        return (0);

 fail:
        error_set(err, "parse pid failed");
        return (-1);
}

int
nsenterat(struct error *err, int fd, int nstype)
{
        if (setns(fd, nstype) < 0) {
                error_set(err, "namespace association failed");
                return (-1);
        }
        return (0);
}

int
nsenter(struct error *err, const char *path, int nstype)
{
        int fd;
        int rv = -1;

        if ((fd = xopen(err, path, O_RDONLY)) < 0)
                return (-1);
        if (setns(fd, nstype) < 0) {
                error_set(err, "namespace association failed: %s", path);
                goto fail;
        }
        rv = 0;

 fail:
        close(fd);
        return (rv);
}

char **
array_new(struct error *err, size_t size)
{
        char **arr = NULL;

        arr = xcalloc(err, size, sizeof(*arr));
        return (arr);
}

void
array_free(char *arr[], size_t size)
{
        if (arr == NULL)
                return;
        for (size_t i = 0; i < size; ++i)
                free(arr[i]);
        free(arr);
}

const char **
array_append(const char **ptr, const char * const arr[], size_t size)
{
        memcpy(ptr, arr, size * sizeof(*arr));
        return (ptr + size);
}

void
array_pack(char *arr[], size_t *size)
{
        char **ptr = arr;
        char **end = arr + *size;
        char *tmp;

        if (arr == NULL)
                return;
        for (*size = 0; ptr < end; ++ptr) {
                if (*ptr != NULL) {
                        tmp = *arr; *arr = *ptr; *ptr = tmp;
                        ++(*size);
                        ++arr;
                }
        }
}

void *
file_map(struct error *err, const char *path, size_t *length)
{
        int fd = -1;
        struct stat s;
        void *p = NULL;

        if ((fd = xopen(err, path, O_RDONLY)) < 0)
                return (NULL);
        if (fstat(fd, &s) < 0)
                goto fail;

        *length = (size_t)s.st_size;
        if ((p = mmap(NULL, *length, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
                p = NULL;
                goto fail;
        }

 fail:
        if (p == NULL)
                error_set(err, "file mapping failed: %s", path);
        close(fd);
        return (p);
}

int
file_unmap(struct error *err, const char *path, void *addr, size_t length)
{
        if (munmap(addr, length) < 0) {
                error_set(err, "file unmapping failed: %s", path);
                return (-1);
        }
        return (0);
}

static mode_t
get_umask(void)
{
        mode_t mask;

        mask = umask(0);
        umask(mask);
        return (mask);
}

static int
set_fsugid(uid_t uid, gid_t gid)
{
        cap_t state = NULL;
        cap_value_t cap = CAP_DAC_OVERRIDE;
        cap_flag_value_t flag;
        int rv = -1;

        setfsgid(gid);
        if ((gid_t)setfsgid((gid_t)-1) != gid) {
                errno = EPERM;
                return (-1);
        }
        setfsuid(uid);
        if ((uid_t)setfsuid((uid_t)-1) != uid) {
                errno = EPERM;
                return (-1);
        }

        /*
         * Changing the filesystem user ID potentially affects effective capabilities.
         * If allowed, restore CAP_DAC_OVERRIDE because some distributions rely on it
         * (e.g. https://bugzilla.redhat.com/show_bug.cgi?id=517575).
         */
        if ((state = cap_get_proc()) == NULL)
                goto fail;
        if (cap_get_flag(state, cap, CAP_PERMITTED, &flag) < 0)
                goto fail;
        if (flag == CAP_SET) {
                if (cap_set_flag(state, CAP_EFFECTIVE, 1, &cap, CAP_SET) < 0)
                        goto fail;
                if (cap_set_proc(state) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        cap_free(state);
        return (rv);
}

static int
make_ancestors(char *path, mode_t perm)
{
        struct stat s;
        char *p;

        if (*path == '\0' || *path == '.')
                return (0);

        if (stat(path, &s) == 0) {
                if (S_ISDIR(s.st_mode))
                        return (0);
                errno = ENOTDIR;
        }
        if (errno != ENOENT)
                return (-1);

        if ((p = strrchr(path, '/')) != NULL) {
                *p = '\0';
                if (make_ancestors(path, perm) < 0)
                        return (-1);
                *p = '/';
        }
        return (mkdir(path, perm));
}

int
file_create(struct error *err, const char *path, void *data, uid_t uid, gid_t gid, mode_t mode)
{
        char *p;
        uid_t euid;
        gid_t egid;
        mode_t perm;
        int fd;
        int rv = -1;

        if ((p = xstrdup(err, path)) == NULL)
                return (-1);

        /*
         * Change the filesystem UID/GID before creating the file to support user namespaces.
         * This is required since Linux 4.8 because the inode needs to be created with a UID/GID known to the VFS.
         */
        euid = geteuid();
        egid = getegid();
        if (set_fsugid(uid, gid) < 0)
                goto fail;

        perm = (0777 & ~get_umask()) | S_IWUSR | S_IXUSR;
        if (make_ancestors(dirname(p), perm) < 0)
                goto fail;
        perm = 0777 & ~get_umask() & mode;

        if (S_ISDIR(mode)) {
                if (mkdir(path, perm) < 0 && errno != EEXIST)
                        goto fail;
        } else if (S_ISLNK(mode)) {
                if (symlink(data, path) < 0 && errno != EEXIST)
                        goto fail;
        } else {
                if ((fd = open(path, O_NOFOLLOW|O_CREAT, perm)) < 0)
                        goto fail;
                close(fd);
        }
        rv = 0;

 fail:
        if (rv < 0)
                error_set(err, "file creation failed: %s", path);

        assert_func(set_fsugid(euid, egid));
        free(p);
        return (rv);
}

static int
do_file_remove(const char *path, const struct stat *s, int flag, maybe_unused struct FTW *ftw)
{
        int ret;
        struct stat sl;

        if (flag == FTW_NS || flag == FTW_DNR)
                return (-1);

        if (flag == FTW_DP) {
                if (rmdir(path) < 0 && errno != ENOTEMPTY)
                        return (-1);
        } else if (flag == FTW_F) {
                if (s->st_size == 0)
                        return (unlink(path));
        } else if (flag == FTW_SL) {
                if ((ret = stat(path, &sl)) < 0 && errno != ENOENT)
                        return (-1);
                if ((ret < 0 && errno == ENOENT) || (ret == 0 && sl.st_size == 0))
                        return (unlink(path));
        }
        return (0);
}

int
file_remove(struct error *err, const char *path)
{
        if (nftw(path, do_file_remove, 1, FTW_MOUNT|FTW_PHYS|FTW_DEPTH) < 0) {
                error_set(err, "file removal failed: %s", path);
                return (-1);
        }
        return (0);
}

int
file_exists(struct error *err, const char *path)
{
        int rv;

        if ((rv = access(path, F_OK)) < 0 && errno != ENOENT) {
                error_set(err, "file lookup failed: %s", path);
                return (-1);
        }
        return (rv == 0);
}

int
file_mode(struct error *err, const char *path, mode_t *mode)
{
        struct stat s;

        if (xstat(err, path, &s) < 0)
                return (-1);
        *mode = s.st_mode;
        return (0);
}

int
file_read_line(struct error *err, const char *path, char *buf, size_t size)
{
        FILE *fs;
        int rv = 0;

        if ((fs = xfopen(err, path, "r")) == NULL)
                return (-1);
        if (fgets(buf, (int)size, fs) == NULL) {
                if (feof(fs))
                        *buf = '\0';
                else {
                        error_setx(err, "file read error: %s", path);
                        rv = -1;
                }
        }
        fclose(fs);
        return (rv);
}

int
file_read_ulong(struct error *err, const char *path, unsigned long *v)
{
        FILE *fs;
        int rv = 0;

        if ((fs = xfopen(err, path, "r")) == NULL)
                return (-1);
        if (fscanf(fs, "%lu", v) != 1) {
                error_setx(err, "file read error: %s", path);
                rv = -1;
        }
        fclose(fs);
        return (rv);
}

int
path_append(struct error *err, char *buf, const char *path)
{
        size_t len, cap;
        int n;

        len = strlen(buf);
        cap = PATH_MAX - len;
        buf += len;

        n = snprintf(buf, cap, "%s%s", (*path == '/') ? "" : "/", path);
        if (n < 0 || (size_t)n >= cap) {
                if ((size_t)n >= cap)
                        errno = ENAMETOOLONG;
                error_set(err, "path error: %s/%s", buf, path);
                return (-1);
        }
        return (0);
}

int
path_join(struct error *err, char *buf, const char *p1, const char *p2)
{
        size_t cap = PATH_MAX;
        int n;

        n = snprintf(buf, cap, "%s%s%s", p1, (*p2 == '/') ? "" : "/", p2);
        if (n < 0 || (size_t)n >= cap) {
                if ((size_t)n >= cap)
                        errno = ENAMETOOLONG;
                error_set(err, "path error: %s/%s", p1, p2);
                return (-1);
        }
        return (0);
}

static int
openrel(struct error *err, int dir, const char *path)
{
        int fd;

        if (*path == '/') {
                if ((fd = xopen(err, path, O_PATH|O_DIRECTORY|O_NOFOLLOW)) < 0)
                        return (-1);
        } else {
                if ((fd = openat(dir, path, O_PATH|O_NOFOLLOW)) < 0) {
                        error_set(err, "open failed: %s", path);
                        return (-1);
                }
        }
        xclose(dir);
        return (fd);
}

int
path_resolve(struct error *err, char *buf, const char *root, const char *path)
{
        int fd = -1;
        int rv = -1;
        char realpath[PATH_MAX];
        char dbuf[2][PATH_MAX];
        char *link = dbuf[0];
        char *ptr = dbuf[1];
        char *file, *p;
        unsigned int noents = 0;
        unsigned int nlinks = 0;
        ssize_t n;

        *ptr = '\0';
        *realpath = '\0';
        assert(*root == '/');

        if ((fd = openrel(err, -1, root)) < 0)
                goto fail;
        if (path_append(err, ptr, path) < 0)
                goto fail;

        while ((file = strsep(&ptr, "/")) != NULL) {
                if (*file == '\0' || !strcmp(file, "."))
                        continue;
                else if (!strcmp(file, "..")) {
                        /*
                         * Remove the last component from the resolved path. If we are not below
                         * non-existent components, restore the previous file descriptor as well.
                         */
                        if ((p = strrchr(realpath, '/')) == NULL) {
                                error_setx(err, "path error: %s resolves outside of %s", path, root);
                                goto fail;
                        }
                        *p = '\0';
                        if (noents > 0)
                                --noents;
                        else {
                                if ((fd = openrel(err, fd, "..")) < 0)
                                        goto fail;
                        }
                } else {
                        if (noents > 0)
                                goto missing_ent;

                        n = readlinkat(fd, file, link, PATH_MAX);
                        if (n > 0 && n < PATH_MAX && nlinks < MAXSYMLINKS) {
                                /*
                                 * Component is a symlink, append the rest of the path to it and
                                 * proceed with the resulting buffer. If it is absolute, also clear
                                 * the resolved path and reset our file descriptor to root.
                                 */
                                link[n] = '\0';
                                if (*link == '/') {
                                        ++link;
                                        *realpath = '\0';
                                        if ((fd = openrel(err, fd, root)) < 0)
                                                goto fail;
                                }
                                if (ptr != NULL) {
                                        if (path_append(err, link, ptr) < 0)
                                                goto fail;
                                }
                                ptr = link;
                                link = dbuf[++nlinks % 2];
                        } else {
                                if (n >= PATH_MAX)
                                        errno = ENAMETOOLONG;
                                else if (nlinks >= MAXSYMLINKS)
                                        errno = ELOOP;
                                switch (errno) {
                                missing_ent:
                                case ENOENT:
                                        /* Component doesn't exist */
                                        ++noents;
                                        if (path_append(err, realpath, file) < 0)
                                                goto fail;
                                        break;
                                case EINVAL:
                                        /* Not a symlink, proceed normally */
                                        if ((fd = openrel(err, fd, file)) < 0)
                                                goto fail;
                                        if (path_append(err, realpath, file) < 0)
                                                goto fail;
                                        break;
                                default:
                                        error_set(err, "path error: %s/%s", root, path);
                                        goto fail;
                                }
                        }
                }
        }
        if (path_join(err, buf, root, realpath) < 0)
                goto fail;
        rv = 0;

 fail:
        xclose(fd);
        return (rv);
}

int
perm_drop_privileges(struct error *err, uid_t uid, gid_t gid, bool drop_groups)
{
        uid_t euid;
        gid_t egid;
        gid_t egroup;

        euid = geteuid();
        egid = getegid();

        if (drop_groups) {
                switch (getgroups(0, NULL)) {
                case -1:
                        goto fail;
                case 0:
                        break;
                case 1:
                        if (getgroups(1, &egroup) < 0)
                                goto fail;
                        if (egroup == gid)
                                break;
                        /* Fallthrough */
                default:
                        if (setgroups(1, &gid) < 0)
                                goto fail;
                        break;
                }
        }
        if (egid != gid && setregid(gid, gid) < 0)
                goto fail;
        if (euid != uid && setreuid(uid, uid) < 0)
                goto fail;
        if ((egid != gid && getegid() != gid) ||
            (euid != uid && geteuid() != uid)) {
                errno = EPERM;
                goto fail;
        }
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0)
                goto fail;
        return (0);

 fail:
        error_set(err, "privilege change failed");
        return (-1);
}

int
perm_drop_bounds(struct error *err)
{
        unsigned long n;
        cap_value_t last_cap = CAP_LAST_CAP;

        if (file_read_ulong(err, PROC_LAST_CAP_PATH, &n) < 0)
                return (-1);
        if ((cap_value_t)n >= 0 && (cap_value_t)n > last_cap)
                last_cap = (cap_value_t)n;

        for (cap_value_t c = 0; c <= last_cap; ++c) {
                if (cap_get_bound(c) > 0 && cap_drop_bound(c) < 0) {
                        error_set(err, "capability change failed");
                        return (-1);
                }
        }
        return (0);
}

int
perm_set_capabilities(struct error *err, cap_flag_t type, const cap_value_t caps[], size_t size)
{
        cap_t state = NULL;
        cap_t tmp = NULL;
        cap_flag_value_t flag;
        int rv = -1;

        if (type == CAP_AMBIENT) {
                /* Ambient capabilities are only supported since Linux 4.3 and are not available in libcap. */
                if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) < 0 && errno != EINVAL)
                        goto fail;
                if (caps != NULL && size > 0) {
                        for (size_t i = 0; i < size; ++i) {
                                if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, caps[i], 0, 0) < 0 && errno != EINVAL)
                                        goto fail;
                        }
                }
                return (0);
        }

        if ((state = cap_get_proc()) == NULL)
                goto fail;
        if (cap_clear_flag(state, type) < 0)
                goto fail;
        if (caps != NULL && size > 0) {
                if (cap_set_flag(state, type, (int)size, caps, CAP_SET) < 0)
                        goto fail;
        }
        if (type == CAP_PERMITTED) {
                if ((tmp = cap_dup(state)) == NULL)
                        goto fail;
                if (cap_clear_flag(state, CAP_EFFECTIVE) < 0)
                        goto fail;
                if (caps != NULL && size > 0) {
                        for (size_t i = 0; i < size; ++i) {
                                if (cap_get_flag(tmp, caps[i], CAP_EFFECTIVE, &flag) < 0)
                                        goto fail;
                                if (cap_set_flag(state, CAP_EFFECTIVE, 1, &caps[i], flag) < 0)
                                        goto fail;
                        }
                }
        }
        if (cap_set_proc(state) < 0)
                goto fail;
        rv = 0;

 fail:
        if (rv < 0)
                error_set(err, "capability change failed");
        cap_free(state);
        cap_free(tmp);
        return (rv);
}
