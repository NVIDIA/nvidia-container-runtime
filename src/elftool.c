/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * This file is part of libnvidia-container.
 * If this file is being compiled and dynamically linked against libelf from the
 * elfutils package (usually characterized by the definition of the macro `WITH_LIBELF'),
 * the following license notice covers the use of said library along with the terms from
 * the COPYING and COPYING.LESSER files:
 *
 *  elfutils is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  elfutils is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received copies of the GNU General Public License and
 *  the GNU Lesser General Public License along with this program.  If
 *  not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <gelf.h>

#include "elftool.h"
#include "error.h"
#include "utils.h"
#include "xfuncs.h"

/* Extracted from <elf.h> since elftoolchain headers conflict. */
#ifndef WITH_LIBELF
# define ELF_NOTE_GNU      "GNU"
# define ELF_NOTE_ABI      1
# define ELF_NOTE_OS_LINUX 0
#endif /* WITH_LIBELF */

static int lookup_section(struct elftool *, GElf_Shdr *, Elf_Scn **, Elf64_Word, const char *);

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
lookup_section(struct elftool *ctx, GElf_Shdr *shdr, Elf_Scn **scn, Elf64_Word type, const char *name)
{
        size_t shstrndx;
        char *shname;

        if (elf_getshdrstrndx(ctx->elf, &shstrndx) < 0)
                goto fail;

        *scn = NULL;
        while ((*scn = elf_nextscn(ctx->elf, *scn)) != NULL) {
                   if (gelf_getshdr(*scn, shdr) == NULL)
                           goto fail;
                   if ((shname = elf_strptr(ctx->elf, shstrndx, shdr->sh_name)) == NULL)
                           goto fail;
                   if (shdr->sh_type == type && name == NULL)
                           return (0);
                   else if (shdr->sh_type == type && str_equal(shname, name))
                           return (0);
        }
        error_setx(ctx->err, "elf section 0x%x missing: %s", type, ctx->path);
        return (-1);

 fail:
        error_set_elf(ctx->err, "elf section read error: %s", ctx->path);
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

        if (lookup_section(ctx, &shdr, &scn, SHT_DYNAMIC, NULL) < 0)
                return (-1);
        if ((data = elf_getdata(scn, NULL)) == NULL)
                goto fail;

        for (size_t i = 0; i < data->d_size / shdr.sh_entsize; ++i) {
                if (gelf_getdyn(data, (int)i, &dyn) == NULL)
                        goto fail;
                if (dyn.d_tag == DT_NEEDED) {
                        if ((dep = elf_strptr(ctx->elf, shdr.sh_link, dyn.d_un.d_ptr)) == NULL)
                                goto fail;
                        if (str_has_prefix(dep, lib))
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
        Elf64_Nhdr nhdr;
        char *ptr;
        uint32_t desc[4];

        if (lookup_section(ctx, &shdr, &scn, SHT_NOTE, ".note.ABI-tag") < 0)
                return (-1);
        if ((data = elf_getdata(scn, NULL)) == NULL) {
                error_set_elf(ctx->err, "elf data read error: %s", ctx->path);
                return (-1);
        }

        if (data->d_size <= sizeof(nhdr))
                return (0);
        memcpy(&nhdr, data->d_buf, sizeof(nhdr));
        ptr = (char *)data->d_buf + sizeof(nhdr);

        if (data->d_size < sizeof(nhdr) + nhdr.n_namesz + nhdr.n_descsz)
                return (0);
        if (nhdr.n_type != ELF_NOTE_ABI || nhdr.n_namesz != sizeof(ELF_NOTE_GNU) || nhdr.n_descsz < sizeof(desc))
                return (0);
        if (memcmp(ptr, ELF_NOTE_GNU, nhdr.n_namesz))
                return (0);
        memcpy(&desc, ptr + nhdr.n_namesz, sizeof(desc));

        return (desc[0] == ELF_NOTE_OS_LINUX && !memcmp(&desc[1], abi, 3 * sizeof(uint32_t)));
}
