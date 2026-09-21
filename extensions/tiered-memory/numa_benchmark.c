#define _GNU_SOURCE
#include <numa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define SIZE (20 * 1024 * 1024) // 20MB for faster runs
#define STRIDE 64

void benchmark_node(int node, int simulate_delay) {
    void* ptr = malloc(SIZE);
    memset(ptr, 0, SIZE); // warm-up

    clock_t start = clock();
    for (int i = 0; i < SIZE; i += STRIDE) {
        ((char*)ptr)[i] += 1;
        if (simulate_delay && (i % (1024 * 1024) == 0)) usleep(100); // only every 1MB
    }
    clock_t end = clock();

    printf("Tier %d (%s): Time = %.2f ms\n", node,
           simulate_delay ? "simulated remote" : "local DRAM",
           1000.0 * (double)(end - start) / CLOCKS_PER_SEC);

    free(ptr);
}

int main() {
    benchmark_node(0, 0); // DRAM
    benchmark_node(1, 1); // Simulated CXL
    return 0;
}
