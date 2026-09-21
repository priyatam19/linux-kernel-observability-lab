#define _POSIX_C_SOURCE 200809L
#include "pagemap.h"
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv)
{
    size_t total = (size_t)2 << 30, chunk, count;
    long page = sysconf(_SC_PAGESIZE);
    char output[64], *end;
    int status = EXIT_FAILURE;
    if (argc < 2 || argc > 3 || page <= 0) goto usage;
    if (argc == 3) {
        errno = 0;
        unsigned long mb = strtoul(argv[2], &end, 10);
        if (errno || *end || argv[2][0] == '-' || !mb || mb > 2048) goto usage;
        total = (size_t)mb << 20;
    }
    if (!strcmp(argv[1], "4KB")) chunk = 4096;
    else if (!strcmp(argv[1], "2MB")) chunk = 2 << 20;
    else if (!strcmp(argv[1], "2GB")) chunk = total;
    else goto usage;
    if (chunk > total || total % chunk || chunk % (size_t)page) goto usage;
    count = total / chunk;
    void **blocks = calloc(count, sizeof(*blocks));
    if (!blocks) { perror("calloc"); return status; }
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) { perror("open pagemap"); free(blocks); return status; }
    snprintf(output, sizeof(output), "latency_log_%s.csv", argv[1]);
    FILE *out = fopen(output, "w");
    if (!out) { perror("fopen"); close(fd); free(blocks); return status; }
    if (fprintf(out, "virtual_address,physical_address,latency_us\n") < 0) goto done;
    for (size_t i = 0; i < count; i++) {
        int err = posix_memalign(&blocks[i], (size_t)page, chunk);
        if (err) { errno = err; perror("posix_memalign"); goto done; }
        volatile unsigned char *bytes = blocks[i];
        for (size_t j = 0; j < chunk; j += (size_t)page) bytes[j] = 1;
    }
    /* Each allocation has its own virtual range; never walk across allocations. */
    for (size_t i = 0; i < count; i++) {
        if (dump_va_pa_with_latency(fd, blocks[i], chunk, (size_t)page, out)) {
            perror("pagemap lookup"); goto done;
        }
    }
    status = EXIT_SUCCESS;
done:
    if (fclose(out)) { perror("fclose"); status = EXIT_FAILURE; }
    for (size_t i = 0; i < count; i++) free(blocks[i]);
    free(blocks);
    close(fd);
    if (status) remove(output);
    return status;
usage:
    fprintf(stderr, "Usage: %s 4KB|2MB|2GB [total_MiB: 1..2048, default 2048]\n", argv[0]);
    return EXIT_FAILURE;
}
