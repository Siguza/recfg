/* Copyright (c) 2020 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#include <stdbool.h>
#include <stddef.h>             // size_t
#include <stdint.h>
#include <stdlib.h>             // strtoull
#include <string.h>             // strncmp

#include "common.h"
#include "util.h"
#include "recfg.h"

enum
{
    kFlagSearch = 0x1,
};

typedef struct
{
    size_t off;
    size_t len;
    uint32_t flags;
} recfg_arg_t;

static int recfg_end_cb(void *a)
{
    LOG("end\n"); // Intentional newline
    return kRecfgSuccess;
}

static int recfg_delay_cb(void *a, uint32_t *delay)
{
    LOG("delay %d", *delay);
    return kRecfgSuccess;
}

static int recfg_read32_cb(void *a, uint64_t *addr, uint32_t *mask, uint32_t *data, bool *retry, uint8_t *recnt)
{
    if(*retry)  LOG("rd32 0x%09llx & 0x%08x == 0x%08x, retry = %d", *addr, *mask, *data, *recnt);
    else        LOG("rd32 0x%09llx & 0x%08x == 0x%08x", *addr, *mask, *data);
    return kRecfgSuccess;
}

static int recfg_read64_cb(void *a, uint64_t *addr, uint64_t *mask, uint64_t *data, bool *retry, uint8_t *recnt)
{
    if(*retry)  LOG("rd64 0x%09llx & 0x%016llx == 0x%016llx, retry = %d", *addr, *mask, *data, *recnt);
    else        LOG("rd64 0x%09llx & 0x%016llx == 0x%016llx", *addr, *mask, *data);
    return kRecfgSuccess;
}

static int recfg_write32_cb(void *a, uint64_t *addr, uint32_t *data)
{
    LOG("wr32 0x%09llx = 0x%08x", *addr, *data);
    return kRecfgSuccess;
}

static int recfg_write64_cb(void *a, uint64_t *addr, uint64_t *data)
{
    LOG("wr64 0x%llx = 0x%016llx", *addr, *data);
    return kRecfgSuccess;
}

static int recfg_do_range(char *mem, size_t size, char *base)
{
    size_t err = 0;
    if(recfg_check(mem, size, &err, true) != kRecfgSuccess)
    {
        ERR("Error at offset 0x%lx (sequence 0x%lx)", mem - base + err, mem - base);
        return -1;
    }
    recfg_cb_t cb =
    {
        .generic = NULL,
        .end     = recfg_end_cb,
        .delay   = recfg_delay_cb,
        .r32     = recfg_read32_cb,
        .r64     = recfg_read64_cb,
        .w32     = recfg_write32_cb,
        .w64     = recfg_write64_cb,
    };
    return recfg_walk(mem, size, &cb, NULL);
}

int recfg(void *mem, size_t size, void *a)
{
    const bool warn = true; // for macros
    int retval = kRecfgFailure;
    recfg_arg_t *arg = a;

    REQ(arg->off <= size);
    REQ(arg->len <= arg->off + size);

    char *ptr = (char*)mem + arg->off;
    size_t len = arg->len ? arg->len : size - arg->off;
    if(arg->flags & kFlagSearch)
    {
        REQ(len >= 0x320);
        REQ(strncmp(ptr + 0x280, "iBoot-", 6) == 0);
        uint64_t base = *(uint64_t*)(ptr + (*(uint32_t*)(ptr + 0x8) == 0x580017c1 /* ldr x1, 0x300 */ ? 0x300 : 0x318)),
                 top  = base + len;
        for(uint64_t *cur = (uint64_t*)(ptr + 0x320), *end = cur + ((len-0x320)/sizeof(*cur)); cur < end; ++cur)
        {
            uint64_t a = cur[ 0],
                     b = cur[-1],
                     c = cur[-2],
                     d = cur[-3];
            if
            (
                a == 0 && b == 0 &&
                (c & 0x1) == 0 && c > 0 && c < 0x10000 && // Completely baseless assumption that sequence parts are never longer
                (d & 0x3) == 0 && d > base && d + c * sizeof(uint32_t) < top
            )
            {
                uint64_t *p = cur - 3;
                while(true)
                {
                    c = p[-1];
                    d = p[-2];
                    if
                    (
                        (c & 0x1) == 0 && c > 0 && c < 0x10000 &&
                        (d & 0x3) == 0 && d > base && d + c * sizeof(uint32_t) < top
                    )
                    {
                        p -= 2;
                    }
                    else
                    {
                        break;
                    }
                }
                for(; p[0] != 0 && p[1] != 0; p += 2)
                {
                    LOG("# 0x%llx 0x%llx", p[0], p[1]);
                    retval = recfg_do_range(ptr + p[0] - base, p[1] * sizeof(uint32_t), ptr);
                    if(retval != 0)
                    {
                        goto out;
                    }
                    LOG("");
                }
            }
        }
    }
    else
    {
        retval = recfg_do_range(ptr, len, ptr);
    }

out:;
    return retval;
}

int main(int argc, const char **argv)
{
    if(argc < 2)
    {
        goto badargs;
    }
    int aoff = 1;
    uint32_t flags = 0;
    unsigned long long off = 0,
                       len = 0;
    for(; aoff < argc; ++aoff)
    {
        if(argv[aoff][0] != '-')
        {
            break;
        }
        for(size_t i = 1; argv[aoff][i] != '\0'; ++i)
        {
            char c = argv[aoff][i];
            switch(c)
            {
                case 's':
                    flags |= kFlagSearch;
                    break;
                default:
                    ERR("Unknown option: -%c", c);
                    return -1;
            }
        }
    }
    if(aoff >= argc)
    {
        goto badargs;
    }
    const char *infile = argv[aoff++];
    if(aoff < argc)
    {
        char *end = NULL;
        off = strtoull(argv[aoff], &end, 0);
        if(end[0] != '\0')
        {
            ERR("Bad offset: %s", argv[aoff]);
            return -1;
        }
        ++aoff;
    }
    if(aoff < argc)
    {
        char *end = NULL;
        len = strtoull(argv[aoff], &end, 0);
        if(end[0] != '\0')
        {
            ERR("Bad length: %s", argv[aoff]);
            return -1;
        }
        ++aoff;
    }
    if(aoff < argc)
    {
        goto badargs;
    }
    recfg_arg_t arg =
    {
        .off = off,
        .len = len,
        .flags = flags,
    };
    return file2mem(infile, &recfg, &arg);

badargs:;
    ERR("Usage: %s [-s] file [off [len]]", argv[0]);
    return -1;
}
