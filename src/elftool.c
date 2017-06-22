/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 */

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <gelf.h>

#include "elftool.h"
#include "error.h"
#include "utils.h"
#include "xfuncs.h"

static int lookup_section(struct elftool *, GElf_Shdr *, Elf_Scn **, Elf64_Word);

void
elftool_init(struct elftool *ctx, struct error *err)
{
        *ctx = (struct elftool){err, -1, NULL, NULL};
        elf_version(EV_CURRENT);
}

int
elftool_open(struct elftool *ctx, const char *path)
{
        if ((ctx->fd = xopen(ctx->err, path, O_RDONLY)) < 0)
                return (-1);
        if ((ctx->elf = elf_begin(ctx->fd, ELF_C_READ, NULL)) == NULL) {
                error_setx(ctx->err, "elf file read error: %s", path);
                xclose(ctx->fd);
                return (-1);
        }
        ctx->path = path;
        return (0);
}

void
elftool_close(struct elftool *ctx)
{
        elf_end(ctx->elf);
        xclose(ctx->fd);

        ctx->elf = NULL;
        ctx->fd = -1;
        ctx->path = NULL;
}

static int
lookup_section(struct elftool *ctx, GElf_Shdr *shdr, Elf_Scn **scn, Elf64_Word type)
{
        *scn = NULL;
        while ((*scn = elf_nextscn(ctx->elf, *scn)) != NULL) {
                   if (gelf_getshdr(*scn, shdr) == NULL) {
                           error_set_elf(ctx->err, "elf section read error: %s", ctx->path);
                           return (-1);
                   }
                   if (shdr->sh_type == type)
                           return (0);
        }
        error_setx(ctx->err, "elf section 0x%x missing: %s", type, ctx->path);
        return (-1);
}

int
elftool_has_dependency(struct elftool *ctx, const char *lib)
{
        GElf_Shdr shdr;
        Elf_Scn *scn;
        Elf_Data *data;
        GElf_Dyn dyn;
        char *dep;

        if (lookup_section(ctx, &shdr, &scn, SHT_DYNAMIC) < 0)
                return (-1);
        if ((data = elf_getdata(scn, NULL)) == NULL)
                goto fail;

        for (size_t i = 0; i < data->d_size / shdr.sh_entsize; ++i) {
                if (gelf_getdyn(data, (int)i, &dyn) == NULL)
                        goto fail;
                if (dyn.d_tag == DT_NEEDED) {
                        if ((dep = elf_strptr(ctx->elf, shdr.sh_link, dyn.d_un.d_ptr)) == NULL)
                                goto fail;
                        if (!strpcmp(dep, lib))
                                return (true);
                }
        }
        return (false);

 fail:
        error_set_elf(ctx->err, "elf data read error: %s", ctx->path);
        return (-1);
}

int
elftool_has_abi(struct elftool *ctx, uint32_t abi[3])
{
        GElf_Shdr shdr;
        Elf_Scn *scn;
        Elf_Data *data;
        Elf_Note *nhdr;
        char *ptr;
        uint32_t desc[3];

        if (lookup_section(ctx, &shdr, &scn, SHT_NOTE) < 0)
                return (-1);
        if ((data = elf_getdata(scn, NULL)) == NULL) {
                error_set_elf(ctx->err, "elf data read error: %s", ctx->path);
                return (-1);
        }

        nhdr = (Elf_Note *)data->d_buf;
        ptr = (char *)data->d_buf + sizeof(*nhdr);
        ptr += nhdr->n_namesz + sizeof(uint32_t); /* skip the name and exec field */
        memcpy(&desc, ptr, sizeof(desc));

        return (!memcmp(abi, desc, sizeof(desc)));
}
