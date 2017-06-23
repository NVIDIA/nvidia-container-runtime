/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsl.h"
#include "utils.h"

#define EXPR_MAX 64

static int evaluate_rule(char *, char *, void *, const struct dsl_rule [], size_t);

struct operator {
        const char *str;
        enum dsl_comparator cmp;
};

static const struct operator operators[] = {
        {"=", EQUAL},
        {"!=", NOT_EQUAL},
        {"<", LESS},
        {"<=", LESS_EQUAL},
        {">", GREATER},
        {">=", GREATER_EQUAL},
};

int
dsl_compare_version(const char *v1, enum dsl_comparator cmp, const char *v2)
{
        char *ptr1, *ptr2;
        unsigned long n1, n2;

        if (strspn(v1, "0123456789.") != strlen(v1))
                return (-1);
        if (strspn(v2, "0123456789.") != strlen(v2))
                return (-1);

        while (*v1 != '\0' && *v2 != '\0') {
                if ((n1 = strtoul(v1, &ptr1, 10)) == ULONG_MAX || v1 == ptr1)
                        return (-1);
                if ((n2 = strtoul(v2, &ptr2, 10)) == ULONG_MAX || v2 == ptr2)
                        return (-1);
                if (n1 != n2) {
                        if (cmp == EQUAL)
                                return (false);
                        if (cmp == NOT_EQUAL)
                                return (true);
                        if (cmp == LESS || cmp == LESS_EQUAL)
                                return (n1 < n2);
                        if (cmp == GREATER || cmp == GREATER_EQUAL)
                                return (n1 > n2);
                }
                v1 = ptr1 + strspn(ptr1, ".");
                v2 = ptr2 + strspn(ptr2, ".");
        }
        v1 += strspn(v1, ".0");
        v2 += strspn(v2, ".0");
        if (cmp == NOT_EQUAL)
                return (*v1 != '\0' || *v2 != '\0');
        if (cmp == EQUAL || cmp == LESS_EQUAL || cmp == GREATER_EQUAL) {
                if (*v1 == '\0' && *v2 == '\0')
                        return (true);
                if (cmp == EQUAL)
                        return (false);
        }
        if (cmp == LESS || cmp == LESS_EQUAL)
                return (*v1 == '\0' && *v2 != '\0');
        if (cmp == GREATER || cmp == GREATER_EQUAL)
                return (*v1 != '\0' && *v2 == '\0');
        return (-1);
}

static int
evaluate_rule(char *buf, char *expr, void *ctx, const struct dsl_rule rules[], size_t size)
{
        size_t i, n;
        char *ptr, *val;
        const struct operator *op;
        int ret;

        /* Parse the expression */
        if ((n = strcspn(expr, "<>=!")) == 0)
                return (-1);
        ptr = expr + n;
        if ((n = strspn(ptr, "<>=!")) == 0)
                return (-1);
        for (i = 0; i < nitems(operators); ++i) {
                if (!strncmp(ptr, operators[i].str, n)) {
                        op = &operators[i];
                        *ptr = '\0';
                        val = ptr + n;
                        break;
                }
        }
        if (i == nitems(operators) || *val == '\0')
                return (-1);

        /* Lookup the rule and evaluate it. */
        for (i = 0; i < size; ++i) {
                if (!strcmp(expr, rules[i].name)) {
                        if ((ret = rules[i].func(ctx, op->cmp, val)) == false) {
                                /* Save the expression formatted for error reporting. */
                                if (snprintf(buf, EXPR_MAX, "%s %s %s", expr, op->str, val) >= EXPR_MAX)
                                        return (-1);
                        }
                        return (ret);
                }
        }
        return (-1);
}

int
dsl_evaluate(struct error *err, char *expr, void *ctx, const struct dsl_rule rules[], size_t size)
{
        char *or_expr, *and_expr;
        int ret = true;
        char buf[EXPR_MAX];

        while ((or_expr = strsep(&expr, " ")) != NULL) {
                if (*or_expr == '\0')
                        continue;
                while ((and_expr = strsep(&or_expr, ",")) != NULL) {
                        if (*and_expr == '\0')
                                continue;
                        if ((ret = evaluate_rule(buf, and_expr, ctx, rules, size)) < 0) {
                                error_setx(err, "invalid expression");
                                return (-1);
                        }
                        if (!ret)
                                break;
                }
                if (and_expr == NULL)
                        return (0);
        }
        if (!ret) {
                error_setx(err, "unsatisfied condition: %s", buf);
                return (-1);
        }
        return (0);
}
