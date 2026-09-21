#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define GB (1024L * 1024L * 1024L)

void write_addrs_to_file(void **ptrs, size_t count, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < count; ++i)
        fprintf(f, "%p\n", ptrs[i]);
    fclose(f);
}

void alloc_loop(const char *label, size_t block_size, size_t total_size) {
    size_t blocks = total_size / block_size;
    void **ptrs = malloc(sizeof(void *) * blocks);
    if (!ptrs) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < blocks; ++i) {
        ptrs[i] = calloc(1, block_size);  // ensures mapping
        if (!ptrs[i]) {
            fprintf(stderr, "calloc failed at block %zu\n", i);
            exit(EXIT_FAILURE);
        }
    }

    char filename[128];
    snprintf(filename, sizeof(filename), "%s_va.txt", label);
    write_addrs_to_file(ptrs, blocks, filename);
    printf(" Created: %s\n", filename);

    printf(" Done allocating %s blocks. Press ENTER to continue...\n", label);
    getchar();

    for (size_t i = 0; i < blocks; ++i)
        free(ptrs[i]);
    free(ptrs);
}

int main() {
    alloc_loop("4KB", 4 * 1024, 2L * GB);
    alloc_loop("2MB", 2L * 1024 * 1024, 2L * GB);

    void *ptr = calloc(1, 2L * GB);
    FILE *f = fopen("2GB_va.txt", "w");
    fprintf(f, "%p\n", ptr);
    fclose(f);
    printf(" Created: 2GB_va.txt\n");

    printf(" Done allocating full 2GB. Press ENTER to continue...\n");
    getchar();
    free(ptr);

    return 0;
}
