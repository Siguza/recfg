/* Copyright (c) 2020 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#ifndef COMMON_H
#define COMMON_H

#ifdef RECFG_IO
#   include <stdio.h>
#   define LOG(str, args...) do { printf(str "\n", ##args); } while(0)
#   define ERR(str, args...) do { fprintf(stderr, "\x1b[1;91m" str "\x1b[0m\n", ##args); } while(0)
#endif

#ifdef ERR
#   define REQ(expr) \
    do \
    { \
        if(!(expr)) \
        { \
            if(warn) ERR("!(" #expr ")"); \
            goto out; \
        } \
    } while(0)
#else
#   define REQ(expr) \
    do \
    { \
        if(!(expr)) \
        { \
            goto out; \
        } \
    } while(0)
#endif

#endif
