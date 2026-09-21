#define _POSIX_C_SOURCE 200809L
#include "pagemap.h"
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

int dump_va_pa_with_latency(int fd, void *base, size_t size, size_t page_size, FILE *out)
{
    for (size_t i = 0; i < size; i += page_size) {
        uintptr_t va = (uintptr_t)base + i;
        uint64_t entry;
        struct timespec start, end;
        off_t offset = (off_t)((va / page_size) * sizeof(entry));
        if (clock_gettime(CLOCK_MONOTONIC, &start)) return -1;
        ssize_t n = pread(fd, &entry, sizeof(entry), offset);
        if (n != sizeof(entry)) {
            if (n >= 0) errno = EIO;
            return -1;
        }
        if (!(entry & (UINT64_C(1) << 63))) {
            errno = ENOENT;
            return -1;
        }
        uint64_t pfn = entry & ((UINT64_C(1) << 55) - 1);
        if (!pfn) {
            /* Modern kernels mask PFNs without CAP_SYS_ADMIN. */
            fprintf(stderr, "PFN is zero: likely masked; run in a privileged lab VM.\n");
            errno = EACCES;
            return -1;
        }
        uint64_t pa = pfn * page_size + va % page_size;
        if (clock_gettime(CLOCK_MONOTONIC, &end)) return -1;
        double us = (end.tv_sec - start.tv_sec) * 1e6 +
                    (end.tv_nsec - start.tv_nsec) / 1e3;
        if (fprintf(out, "0x%" PRIxPTR ",0x%" PRIx64 ",%.3f\n", va, pa, us) < 0)
            return -1;
    }
    return 0;
}
