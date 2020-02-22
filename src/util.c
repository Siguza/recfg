/* Copyright (c) 2020 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#include <fcntl.h>              // open
#include <stddef.h>             // size_t
#include <unistd.h>             // close
#include <sys/mman.h>           // mmap, munmap
#include <sys/stat.h>           // fstat

#include "common.h"
#include "util.h"

int file2mem(const char *path, int (*func)(void*, size_t, void*), void *arg)
{
    int retval = -1;
    int fd = -1;
    void *mem = MAP_FAILED;
    size_t size = 0;

    fd = open(path, O_RDONLY);
    REQ(fd != -1);

    struct stat s;
    REQ(fstat(fd, &s) == 0);
    size = s.st_size;

    mem = mmap(NULL, size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    REQ(mem != MAP_FAILED);

    retval = func(mem, size, arg);
out:;
    if(mem != MAP_FAILED) munmap(mem, size);
    if(fd != -1) close(fd);
    return retval;
}
