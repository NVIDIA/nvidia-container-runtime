/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
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
#include <pwd.h>
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
static int open_next(struct error *, int, const char *);
static int do_path_resolve(struct error *, bool, char *, const char *, const char *);

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
str_lower(char *str)
{
        for (char *p = str; *p != '\0'; ++p)
                *p = (char)tolower(*p);
}

bool
str_equal(const char *s1, const char *s2)
{
        return (!strcmp(s1, s2));
}

bool
str_case_equal(const char *s1, const char *s2)
{
        return (!strcasecmp(s1, s2));
}

bool
str_has_prefix(const char *str, const char *prefix)
{
        return (!strncmp(str, prefix, strlen(prefix)));
}

bool
str_has_suffix(const char *str, const char *suffix)
{
        size_t len, slen;

        len = strlen(str);
        slen = strlen(suffix);
        return ((len >= slen) ? str_equal(str + len - slen, suffix) : false);
}

bool
str_empty(const char *str)
{
        return (str != NULL && *str == '\0');
}

bool
str_array_match_prefix(const char *str, const char * const arr[], size_t size)
{
        for (size_t i = 0; i < size; ++i) {
                if (str_has_prefix(str, arr[i]))
                        return (true);
        }
        return (false);
}

bool
str_array_match(const char *str, const char * const arr[], size_t size)
{
        for (size_t i = 0; i < size; ++i) {
                if (str_equal(str, arr[i]))
                        return (true);
        }
        return (false);
}

int
str_join(struct error *err, char **s1, const char *s2, const char *sep)
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
str_to_pid(struct error *err, const char *str, pid_t *pid)
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
str_to_ugid(struct error *err, char *str, uid_t *uid, gid_t *gid)
{
        char *ptr;
        uintmax_t n;
        struct passwd *passwd = NULL;
        struct group *group = NULL;

        n = strtoumax(str, &ptr, 10);
        if (ptr != str) {
                if (*ptr != '\0' && *ptr != ':') {
                        errno = EINVAL;
                        goto fail;
                }
                if (n == UINTMAX_MAX || n != (uid_t)n) {
                        errno = ERANGE;
                        goto fail;
                }
                if (*ptr == ':')
                        ++ptr;
                *uid = (uid_t)n;
        } else {
                /* Not a numeric UID, check for a username. */
                if ((ptr = strchr(str, ':')) != NULL)
                        *ptr++ = '\0';
                if ((passwd = getpwnam(str)) == NULL) {
                        errno = ENOENT;
                        goto fail;
                }
                *uid = passwd->pw_uid;
        }

        str = ptr;
        if (str == NULL || *str == '\0') {
                /* No group specified, infer it from the UID */
                if (passwd == NULL && (passwd = getpwuid(*uid)) == NULL) {
                        errno = ENOENT;
                        goto fail;
                }
                *gid = passwd->pw_gid;
                return (0);
        }

        n = strtoumax(str, &ptr, 10);
        if (ptr != str) {
                if (*ptr != '\0') {
                        errno = EINVAL;
                        goto fail;
                }
                if (n == UINTMAX_MAX || n != (gid_t)n) {
                        errno = ERANGE;
                        goto fail;
                }
                *gid = (gid_t)n;
        } else {
                /* Not a numeric GID, check for a groupname. */
                if ((group = getgrnam(str)) == NULL) {
                        errno = ENOENT;
                        goto fail;
                }
                *gid = group->gr_gid;
        }
        return (0);

 fail:
        error_set(err, "parse user/group failed");
        return (-1);
}

int
ns_enter_at(struct error *err, int fd, int nstype)
{
        if (setns(fd, nstype) < 0) {
                error_set(err, "namespace association failed");
                return (-1);
        }
        return (0);
}

int
ns_enter(struct error *err, const char *path, int nstype)
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
        char **arr;

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

char **
array_copy(struct error *err, const char * const arr[], size_t size)
{
        char **ptr;

        if ((ptr = array_new(err, size)) == NULL)
                return (NULL);
        array_append((const char **)ptr, arr, size);
        return (ptr);
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

size_t
array_size(const char * const arr[])
{
        size_t n = 0;

        while (*arr++ != NULL)
                ++n;
        return (n);
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
file_create(struct error *err, const char *path, const char *data, uid_t uid, gid_t gid, mode_t mode)
{
        char *p;
        uid_t euid;
        gid_t egid;
        mode_t perm;
        int fd;
        size_t size;
        int flags = O_NOFOLLOW|O_CREAT;
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
                if (data == NULL) {
                        errno = EINVAL;
                        goto fail;
                }
                if (symlink(data, path) < 0 && errno != EEXIST)
                        goto fail;
        } else {
                if (data != NULL) {
                        size = strlen(data);
                        flags |= O_WRONLY|O_TRUNC;
                }
                if ((fd = open(path, flags, perm)) < 0) {
                        if (errno == ELOOP)
                                errno = EEXIST; /* XXX Better error message if the file exists and is a symlink. */
                        goto fail;
                }
                if (data != NULL && write(fd, data, size) < (ssize_t)size) {
                        close(fd);
                        goto fail;
                }
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
file_exists_at(struct error *err, const char *dir, const char *path)
{
        int fd;
        int rv;

        if ((fd = xopen(err, dir, O_PATH|O_DIRECTORY)) < 0)
                return (-1);
        if ((rv = faccessat(fd, (*path == '/') ? path + 1 : path, F_OK, 0)) < 0 && errno != ENOENT) {
                error_set(err, "file lookup failed: %s", path);
                close(fd);
                return (-1);
        }
        close(fd);
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
file_read_text(struct error *err, const char *path, char **txt)
{
        FILE *fs;
        size_t n;
        char buf[512];
        int rv = -1;

        if ((fs = xfopen(err, path, "r")) == NULL)
                return (-1);
        *txt = NULL;
        while ((n = fread(buf, 1, sizeof(buf) - 1, fs)) > 0) {
                buf[n] = '\0';
                if (str_join(err, txt, buf, "") < 0)
                        goto fail;
        }
        if (feof(fs))
                rv = 0;
        else
                error_setx(err, "file read error: %s", path);

 fail:
        if (rv < 0)
                free(*txt);
        fclose(fs);
        return (rv);
}

int
file_read_uint32(struct error *err, const char *path, uint32_t *v)
{
        FILE *fs;
        int rv = 0;

        if ((fs = xfopen(err, path, "r")) == NULL)
                return (-1);
        if (fscanf(fs, "%"PRIu32, v) != 1) {
                error_setx(err, "file read error: %s", path);
                rv = -1;
        }
        fclose(fs);
        return (rv);
}

int
path_new(struct error *err, char *buf, const char *path)
{
        *buf = '\0';
        return (path_append(err, buf, path));
}

int
path_append(struct error *err, char *buf, const char *path)
{
        size_t len, cap;
        char *end;
        int n;

        if (str_empty(path))
                return (0);

        len = strlen(buf);
        cap = PATH_MAX - len;
        end = (len > 0) ? buf + len - 1 : buf;
        buf += len;

        n = snprintf(buf, cap, "%s%s", (*path != '/' && *end != '/') ? "/" : "",
                                       (*path == '/' && *end == '/') ? path + 1 : path);
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
        if (path_new(err, buf, p1) < 0)
                return (-1);
        return (path_append(err, buf, p2));
}

static int
open_next(struct error *err, int dir, const char *path)
{
        int fd;
        int flags = O_PATH|O_NOFOLLOW;

        if (*path == '/')
                flags |= O_DIRECTORY;
        if ((fd = openat(dir, path, flags)) < 0) {
                error_set(err, "open failed: %s", path);
                return (-1);
        }
        xclose(dir);
        return (fd);
}

static int
do_path_resolve(struct error *err, bool full, char *buf, const char *root, const char *path)
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

        if ((fd = open_next(err, -1, root)) < 0)
                goto fail;
        if (path_append(err, ptr, path) < 0)
                goto fail;

        while ((file = strsep(&ptr, "/")) != NULL) {
                if (*file == '\0' || str_equal(file, "."))
                        continue;
                else if (str_equal(file, "..")) {
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
                                if ((fd = open_next(err, fd, "..")) < 0)
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
                                        if ((fd = open_next(err, fd, root)) < 0)
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
                                        if ((fd = open_next(err, fd, file)) < 0)
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

        if (!full) {
                if (path_new(err, buf, realpath) < 0)
                        goto fail;
        } else {
                if (path_join(err, buf, root, realpath) < 0)
                        goto fail;
        }
        rv = 0;

 fail:
        xclose(fd);
        return (rv);
}

int
path_resolve(struct error *err, char *buf, const char *root, const char *path)
{
        return (do_path_resolve(err, false, buf, root, path));
}

int
path_resolve_full(struct error *err, char *buf, const char *root, const char *path)
{
        return (do_path_resolve(err, true, buf, root, path));
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
perm_set_bounds(struct error *err, const cap_value_t caps[], size_t size)
{
        uint32_t n;
        cap_value_t last_cap = CAP_LAST_CAP;

        if (file_read_uint32(err, PROC_LAST_CAP_PATH, &n) < 0)
                return (-1);
        if ((cap_value_t)n >= 0 && (cap_value_t)n > last_cap)
                last_cap = (cap_value_t)n;

        for (cap_value_t c = 0; c <= last_cap; ++c) {
                for (size_t i = 0; caps != NULL && i < size; ++i) {
                        if (caps[i] == c)
                                goto next;
                }
                if (prctl(PR_CAPBSET_READ, c) > 0 && prctl(PR_CAPBSET_DROP, c) < 0) {
                        error_set(err, "capability change failed");
                        return (-1);
                }
         next:;
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
                for (size_t i = 0; caps != NULL && i < size; ++i) {
                        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, caps[i], 0, 0) < 0 && errno != EINVAL)
                                goto fail;
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
                for (size_t i = 0; caps != NULL && i < size; ++i) {
                        if (cap_get_flag(tmp, caps[i], CAP_EFFECTIVE, &flag) < 0)
                                goto fail;
                        if (cap_set_flag(state, CAP_EFFECTIVE, 1, &caps[i], flag) < 0)
                                goto fail;
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
