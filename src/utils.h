/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef HEADER_UTILS_H
#define HEADER_UTILS_H

#include <sys/capability.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "error.h"

#define CAP_AMBIENT (cap_flag_t)-1

#define quote_str(...) #__VA_ARGS__
#define nitems(x) (sizeof(x) / sizeof(*x))
#define maybe_unused __attribute__((unused))
#define assert_func(fn) do {      \
        maybe_unused int r_ = fn; \
        assert(r_ == 0);          \
} while (0)

#define MODE_DIR(mode) ((mode) | S_IFDIR)
#define MODE_REG(mode) ((mode) | S_IFREG)
#define MODE_LNK(mode) ((mode) | S_IFLNK)

bool log_active(void);
void log_open(const char *);
void log_close(void);
void log_write(char, const char *, unsigned long, const char *, ...)
    __attribute__((format(printf, 4, 5), nonnull(4)));
int  log_pipe_output(struct error *, int[2]);

#define log_info(msg) log_write('I', __FILE__, __LINE__, msg)
#define log_warn(msg) log_write('W', __FILE__, __LINE__, msg)
#define log_err(msg)  log_write('E', __FILE__, __LINE__, msg)
#define log_infof(fmt, ...) log_write('I', __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_warnf(fmt, ...) log_write('W', __FILE__, __LINE__, fmt, __VA_ARGS__)
#define log_errf(fmt, ...)  log_write('E', __FILE__, __LINE__, fmt, __VA_ARGS__)

void str_lower(char *);
bool str_equal(const char *, const char *);
bool str_case_equal(const char *, const char *);
bool str_has_prefix(const char *, const char *);
bool str_has_suffix(const char *, const char *);
bool str_empty(const char *);
bool str_array_match_prefix(const char *, const char * const [], size_t);
bool str_array_match(const char *, const char * const [], size_t);
int  str_to_pid(struct error *, const char *, pid_t *);
int  str_to_ugid(struct error *, char *, uid_t *, gid_t *);
int  str_join(struct error *, char **, const char *, const char *);

int ns_enter_at(struct error *, int, int);
int ns_enter(struct error *, const char *, int);

char **array_new(struct error *, size_t);
void array_free(char *[], size_t);
void array_pack(char *[], size_t *);
char **array_copy(struct error *, const char * const [], size_t);
size_t array_size(const char * const []);
const char **array_append(const char **, const char * const [], size_t);

void *file_map(struct error *, const char *, size_t *);
int  file_unmap(struct error *, const char *, void *, size_t);
int  file_create(struct error *, const char *, const char *, uid_t, gid_t, mode_t);
int  file_remove(struct error *, const char *);
int  file_exists(struct error *, const char *);
int  file_exists_at(struct error *, const char *, const char *);
int  file_mode(struct error *, const char *, mode_t *);
int  file_read_line(struct error *, const char *, char *, size_t);
int  file_read_text(struct error *, const char *, char **);
int  file_read_uint32(struct error *, const char *, uint32_t *);

int path_new(struct error *, char *, const char *);
int path_append(struct error *, char *, const char *);
int path_join(struct error *, char *, const char *, const char *);
int path_resolve(struct error *, char *, const char *, const char *);
int path_resolve_full(struct error *, char *, const char *, const char *);

int perm_drop_privileges(struct error *, uid_t, gid_t, bool);
int perm_set_bounds(struct error *, const cap_value_t [], size_t);
int perm_set_capabilities(struct error *, cap_flag_t, const cap_value_t [], size_t);

#endif /* HEADER_UTILS_H */
