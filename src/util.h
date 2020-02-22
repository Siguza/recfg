/* Copyright (c) 2020 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>             // size_t

int file2mem(const char *path, int (*func)(void*, size_t, void*), void *arg);

#endif
