/*
* IotaOS - mman.c
* Copyright (c) 2026 grish-ka
* Licensed under the MIT License.
*/

#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/*
 * There is no page sharing between processes, so a mapping is a heap copy:
 * anonymous memory is zeroed, files are read in. Writes never reach the file.
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    (void)addr;
    (void)prot;
    if (length == 0) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    char *mem = calloc(1, length);
    if (!mem) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    if (flags & MAP_ANONYMOUS) return mem;

    if (lseek(fd, offset, SEEK_SET) < 0) {
        free(mem);
        return MAP_FAILED;
    }
    size_t done = 0;
    while (done < length) {
        ssize_t n = read(fd, mem + done, length - done);
        if (n < 0) {
            free(mem);
            return MAP_FAILED;
        }
        if (n == 0) break; /* past the end of the file: the rest stays zero */
        done += n;
    }
    return mem;
}

int munmap(void *addr, size_t length) {
    (void)length;
    free(addr);
    return 0;
}
