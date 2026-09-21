#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define GB (1024L * 1024L * 1024L)
#define HALF_GB (512L * 1024L * 1024L)
#define QUARTER_GB (256L * 1024L * 1024L)
#define PAGE_SIZE 4096

int main() {
    // Step 1: Notify kernel to allocate 1GB and start monitoring
    FILE *f = fopen("/proc/lkp25p3", "w");
    if (f) {
        fprintf(f, "alloc %d\n", getpid());
        fclose(f);
        printf("Sent 'alloc' to kernel module.\n");
    } else {
        perror("fopen /proc/lkp25p3");
        return 1;
    }

    // Step 2: Allocate 512MB buffer
    char *buffer = malloc(HALF_GB);
    if (!buffer) {
        perror("malloc");
        return 1;
    }

    printf("Allocated 512MB at virtual address: %p\n", buffer);
    printf("PID: %d\n", getpid());

    // Step 3: Access loop for 120 seconds
    for (int sec = 0; sec < 120; ++sec) {
        struct timespec start, current;
        clock_gettime(CLOCK_MONOTONIC, &start);

        while (1) {
            clock_gettime(CLOCK_MONOTONIC, &current);
            if ((current.tv_sec - start.tv_sec) >= 1)
                break;

            // Access [0, 256MB] as fast as possible
            for (size_t i = 0; i < QUARTER_GB; i += PAGE_SIZE)
                buffer[i]++;

            // Access [256MB, 512MB] every 1ms
            usleep(1000);
            for (size_t i = QUARTER_GB; i < HALF_GB; i += PAGE_SIZE)
                buffer[i]++;
        }
    }

    printf("Finished 120s access pattern.\n");
    free(buffer);
    return 0;
}
