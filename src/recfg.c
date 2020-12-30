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

#include "common.h"
#include "recfg.h"

#ifdef RECFG_VOLATILE
#   define VOLATILE volatile
#else
#   define VOLATILE
#endif

int recfg_check(void *mem, size_t size, size_t *offp)
{
    int retval = kRecfgFailure;
    char *start = mem,
         *end   = start + size;
    recfg_cmd_t *cmd = mem;
    while(end - (char*)cmd != 0) // != rather than > because ptrdiff is signed
    {
        REQ(end - (char*)cmd >= sizeof(recfg_cmd_t));
        switch(RECFG_CMD_CMD_r(cmd))
        {
            case kRecfgMeta:
                switch(RECFG_CMD_META_r(cmd))
                {
                    case kRecfgEnd:
                        REQ(RECFG_CMD_DATA_r(cmd) == 0);
                        goto end;
                    case kRecfgDelay:
                        break;
                    default:
                        REQ(false);
                }
                cmd = cmd + 1;
                break;
            case kRecfgRead:
                REQ(end - (char*)cmd >= sizeof(recfg_read_t));
                recfg_read_t *read = (recfg_read_t*)cmd;
                REQ(RECFG_READ_COUNT_r(read) == 0);
                // This can happen, and doesn't matter, I guess
                //REQ(RECFG_READ_RETRY_r(read) || RECFG_READ_RECNT_r(read) == 0);
                // This also happens, but I'm pretty sure Apple fucked up
                //REQ(read->__res == 0);
                if(!RECFG_READ_LARGE_r(read))
                {
                    REQ(end - (char*)cmd >= sizeof(recfg_read32_t));
                    cmd = (recfg_cmd_t*)((recfg_read32_t*)read + 1);
                }
                else
                {
                    REQ(end - (char*)cmd >= sizeof(recfg_read64_t) + 2 * sizeof(uint64_t));
                    recfg_read64_t *r64 = (recfg_read64_t*)read;
                    VOLATILE uint32_t *tmp = (VOLATILE uint32_t*)(r64 + 1);
                    if(
                        *tmp == 0xdeadbeef
#ifdef RECFG_VOLATILE
                        // In real memory, 64-bit stuff has to be 64-bit aligned.
                        // When extracted from iBoot though, it only has to be 32-bit aligned.
                        && ((uintptr_t)tmp & 0x4) != 0
#endif
                    )
                    {
                        REQ(end - (char*)cmd >= sizeof(recfg_read64_t) + 2 * sizeof(uint64_t) + sizeof(uint32_t));
                        ++tmp;
                    }
                    VOLATILE uint64_t *datap = (VOLATILE uint64_t*)tmp;
                    cmd = (recfg_cmd_t*)(datap + 2);
                }
                break;
            case kRecfgWrite32:
                {
                    uint32_t cnt, alcnt;
                    REQ(end - (char*)cmd >= sizeof(recfg_write32_t));
                    recfg_write32_t *w32 = (recfg_write32_t*)cmd;
                    cnt = RECFG_WRITE_COUNT_r(w32) + 1;
                    alcnt = (cnt + 3) & ~3;
                    REQ(cnt <= 16 && alcnt <= 16 && (alcnt & 3) == 0); // Sanity
                    REQ(end - (char*)cmd >= sizeof(recfg_write32_t) + alcnt * sizeof(uint8_t) + cnt * sizeof(uint32_t));
                    cmd = (recfg_cmd_t*)((VOLATILE uint32_t*)((VOLATILE uint8_t*)(w32 + 1) + alcnt) + cnt);
                }
                break;
            case kRecfgWrite64:
                {
                    uint32_t cnt, alcnt;
                    REQ(end - (char*)cmd >= sizeof(recfg_write64_t));
                    recfg_write64_t *w64 = (recfg_write64_t*)cmd;
                    cnt = RECFG_WRITE_COUNT_r(w64) + 1;
                    alcnt = (cnt + 3) & ~3;
                    REQ(cnt <= 16 && alcnt <= 16 && (alcnt & 3) == 0); // Sanity
                    REQ(end - (char*)cmd >= sizeof(recfg_write64_t) + alcnt * sizeof(uint8_t) + cnt * sizeof(uint64_t));
                    VOLATILE uint32_t *tmp = (VOLATILE uint32_t*)((VOLATILE uint8_t*)(w64 + 1) + alcnt);
                    if(
                        *tmp == 0xdeadbeef
#ifdef RECFG_VOLATILE
                        // In real memory, 64-bit stuff has to be 64-bit aligned.
                        // When extracted from iBoot though, it only has to be 32-bit aligned.
                        && ((uintptr_t)tmp & 0x4) != 0
#endif
                    )
                    {
                        REQ(end - (char*)cmd >= sizeof(recfg_write64_t) + alcnt * sizeof(uint8_t) + sizeof(uint32_t) + cnt * sizeof(uint64_t));
                        ++tmp;
                    }
                    VOLATILE uint64_t *datap = (VOLATILE uint64_t*)tmp;
                    cmd = (recfg_cmd_t*)(datap + cnt);
                }
                break;
            default:
                // This should REALLY be unreachable, but I don't trust anything in this world.
                REQ(false);
        }
    }
end:;
    retval = kRecfgSuccess;

out:;
    if(offp) *offp = (char*)cmd - start;
    return retval;
}

int recfg_walk(void *mem, size_t size, const recfg_cb_t *cb, void *a)
{
    int retval = kRecfgFailure,
        ret    = kRecfgSuccess;
    char *start = mem,
                  *end   = start + size;
    recfg_cmd_t *cmd = mem;
    while(end - (char*)cmd != 0) // != rather than > because ptrdiff is signed
    {
        if(cb->generic)
        {
            // Make copy on memory that doesn't require volatile access
            recfg_cmd_t copy = *cmd;
            int r = cb->generic(a, &copy);
            REQ(r != kRecfgUpdate);
            if(r != kRecfgSuccess)
            {
                retval = r;
                goto out;
            }
        }
        switch(RECFG_CMD_CMD_r(cmd))
        {
            case kRecfgMeta:
                switch(RECFG_CMD_META_r(cmd))
                {
                    case kRecfgEnd:
                        if(cb->end)
                        {
                            int r = cb->end(a);
                            REQ(r != kRecfgUpdate);
                            if(r != kRecfgSuccess)
                            {
                                retval = r;
                                goto out;
                            }
                        }
                        goto end;
                    case kRecfgDelay:
                        if(cb->delay)
                        {
                            uint32_t data = RECFG_CMD_DATA_r(cmd);
                            int r = cb->delay(a, &data);
                            if(r == kRecfgUpdate)
                            {
                                REQ(data < (1 << 26));
                                RECFG_CMD_DATA_w(cmd, data);
                                ret |= kRecfgUpdate;
                            }
                            else if(r != kRecfgSuccess)
                            {
                                retval = r;
                                goto out;
                            }
                        }
                        break;
                    default:
                        goto out;
                }
                cmd = cmd + 1;
                break;
            case kRecfgRead:
                {
                    recfg_read_t *read = (recfg_read_t*)cmd;
                    if(!RECFG_READ_LARGE_r(read))
                    {
                        recfg_read32_t *r32 = (recfg_read32_t*)read;
                        if(cb->r32)
                        {
                            uint64_t addr = ((uint64_t)RECFG_READ_BASE_r(r32) << 10) | ((uint64_t)RECFG_READ_OFF_r(r32) << 2);
                            uint32_t mask = r32->mask;
                            uint32_t data = r32->data;
                            bool retry = !!RECFG_READ_RETRY_r(r32);
                            uint8_t recnt = RECFG_READ_RECNT_r(r32);
                            int r = cb->r32(a, &addr, &mask, &data, &retry, &recnt);
                            if(r == kRecfgUpdate)
                            {
                                REQ((addr & 0xfffffff000000003) == 0);
                                RECFG_READ_BASE_w(r32, addr >> 10);
                                RECFG_READ_OFF_w(r32, (addr >> 2) & 0xff);
                                r32->mask = mask;
                                r32->data = data;
                                RECFG_READ_RETRY_w(r32, retry ? 1 : 0);
                                RECFG_READ_RECNT_w(r32, recnt);
                                ret |= kRecfgUpdate;
                            }
                            else if(r != kRecfgSuccess)
                            {
                                retval = r;
                                goto out;
                            }
                        }
                        cmd = (recfg_cmd_t*)(r32 + 1);
                    }
                    else
                    {
                        recfg_read64_t *r64 = (recfg_read64_t*)read;
                        VOLATILE uint32_t *tmp = (VOLATILE uint32_t*)(r64 + 1);
                        if(
                            *tmp == 0xdeadbeef
#ifdef RECFG_VOLATILE
                            // In real memory, 64-bit stuff has to be 64-bit aligned.
                            // When extracted from iBoot though, it only has to be 32-bit aligned.
                            && ((uintptr_t)tmp & 0x4) != 0
#endif
                        )
                        {
                            ++tmp;
                        }
                        VOLATILE uint64_t *datap = (VOLATILE uint64_t*)tmp;
                        if(cb->r64)
                        {
                            uint64_t addr = ((uint64_t)RECFG_READ_BASE_r(r64) << 10) | ((uint64_t)RECFG_READ_OFF_r(r64) << 2);
                            uint64_t mask = datap[0];
                            uint64_t data = datap[1];
                            bool retry = !!RECFG_READ_RETRY_r(r64);
                            uint8_t recnt = RECFG_READ_RECNT_r(r64);
                            int r = cb->r64(a, &addr, &mask, &data, &retry, &recnt);
                            if(r == kRecfgUpdate)
                            {
                                REQ((addr & 0xfffffff000000003) == 0);
                                RECFG_READ_BASE_w(r64, addr >> 10);
                                RECFG_READ_OFF_w(r64, (addr >> 2) & 0xff);
                                datap[0] = mask;
                                datap[1] = data;
                                RECFG_READ_RETRY_w(r64, retry ? 1 : 0);
                                RECFG_READ_RECNT_w(r64, recnt);
                                ret |= kRecfgUpdate;
                            }
                            else if(r != kRecfgSuccess)
                            {
                                retval = r;
                                goto out;
                            }
                        }
                        cmd = (recfg_cmd_t*)(datap + 2);
                    }
                }
                break;
            case kRecfgWrite32:
                {
                    uint32_t cnt, alcnt;
                    recfg_write32_t *w32 = (recfg_write32_t*)cmd;
                    cnt = RECFG_WRITE_COUNT_r(w32) + 1;
                    alcnt = (cnt + 3) & ~3;
                    VOLATILE uint32_t *datap = (VOLATILE uint32_t*)((VOLATILE uint8_t*)(w32 + 1) + alcnt);
                    if(cb->w32)
                    {
                        for(uint32_t i = 0; i < cnt; ++i)
                        {
                            uint64_t addr = ((uint64_t)RECFG_WRITE_BASE_r(w32) << 10) | ((uint64_t)RECFG_WRITE_OFF_r(w32, i) << 2);
                            uint32_t data = datap[i];
                            int r = cb->w32(a, &addr, &data);
                            if(r == kRecfgUpdate)
                            {
                                REQ((addr & 0xfffffff000000003) == 0);
                                if(cnt == 1)
                                {
                                    RECFG_WRITE_BASE_w(w32, addr >> 10);
                                }
                                else
                                {
                                    REQ((addr & 0xffffffc00) == (RECFG_WRITE_BASE_r(w32) << 10));
                                }
                                RECFG_WRITE_OFF_w(w32, i, (addr >> 2) & 0xff);
                                datap[i] = data;
                                ret |= kRecfgUpdate;
                            }
                            else if(r != kRecfgSuccess)
                            {
                                retval = r;
                                goto out;
                            }
                        }
                    }
                    cmd = (recfg_cmd_t*)(datap + cnt);
                }
                break;
            case kRecfgWrite64:
                {
                    uint32_t cnt, alcnt;
                    recfg_write64_t *w64 = (recfg_write64_t*)cmd;
                    cnt = RECFG_WRITE_COUNT_r(w64) + 1;
                    alcnt = (cnt + 3) & ~3;
                    VOLATILE uint32_t *tmp = (VOLATILE uint32_t*)((VOLATILE uint8_t*)(w64 + 1) + alcnt);
                    if(
                        *tmp == 0xdeadbeef
#ifdef RECFG_VOLATILE
                        // In real memory, 64-bit stuff has to be 64-bit aligned.
                        // When extracted from iBoot though, it only has to be 32-bit aligned.
                        && ((uintptr_t)tmp & 0x4) != 0
#endif
                    )
                    {
                        ++tmp;
                    }
                    VOLATILE uint64_t *datap = (VOLATILE uint64_t*)tmp;
                    if(cb->w64)
                    {
                        for(uint32_t i = 0; i < cnt; ++i)
                        {
                            uint64_t addr = ((uint64_t)RECFG_WRITE_BASE_r(w64) << 10) | ((uint64_t)RECFG_WRITE_OFF_r(w64, i) << 2);
                            uint64_t data = datap[i];
                            int r = cb->w64(a, &addr, &data);
                            if(r == kRecfgUpdate)
                            {
                                REQ((addr & 0xfffffff000000003) == 0);
                                if(cnt == 1)
                                {
                                    RECFG_WRITE_BASE_w(w64, addr >> 10);
                                }
                                else
                                {
                                    REQ((addr & 0xffffffc00) == (RECFG_WRITE_BASE_r(w64) << 10));
                                }
                                RECFG_WRITE_OFF_w(w64, i, (addr >> 2) & 0xff);
                                datap[i] = data;
                                ret |= kRecfgUpdate;
                            }
                            else if(r != kRecfgSuccess)
                            {
                                retval = r;
                                goto out;
                            }
                        }
                    }
                    cmd = (recfg_cmd_t*)(datap + cnt);
                }
                break;
            default:
                goto out;
        }
    }
end:;
    retval = ret;

out:;
    return retval;
}
