#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


void print_usage(const char *progname) {
    printf("Usage: %s <size_in_MB> <tier: hot | cold | persistent>\n", progname);
}

void log_to_csv(const char *tier, int size_MB, double seconds) {
    FILE *log = fopen("results.csv", "a");
    if (!log) {
        perror("fopen for results.csv failed");
        return;
    }
    fprintf(log, "%s,%d,%.3f\n", tier, size_MB, seconds);
    fclose(log);
}


void print_energy_consumption(const char *tier, int size_MB) {
    double joules_per_MB;

    if (strcmp(tier, "hot") == 0) {
        joules_per_MB = 0.003;  // DRAM
    } else if (strcmp(tier, "cold") == 0) {
        joules_per_MB = 0.012;  // Simulated remote
    } else if (strcmp(tier, "persistent") == 0) {
        joules_per_MB = 0.005;  // NVM
    } else {
        return;
    }

    double total_joules = joules_per_MB * size_MB;
    printf("Estimated energy consumption for '%s': %.3f Joules\n", tier, total_joules);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--benchmark") == 0) {
        int size_MB = atoi(argv[2]);
        if (size_MB <= 0) {
            print_usage(argv[0]);
            return 1;
        }

        char *fake_argv_hot[] = { argv[0], argv[2], "hot", NULL };
        char *fake_argv_cold[] = { argv[0], argv[2], "cold", NULL };
        char *fake_argv_persistent[] = { argv[0], argv[2], "persistent", NULL };

        printf("Running benchmark for %d MB across all tiers...\n\n", size_MB);
        main(3, fake_argv_hot);
        main(3, fake_argv_cold);
        main(3, fake_argv_persistent);
        return 0;
    }

    int size_MB = atoi(argv[1]);
    char *tier = argv[2];

    if (size_MB <= 0 ||
        (strcmp(tier, "hot") != 0 && strcmp(tier, "cold") != 0 && strcmp(tier, "persistent") != 0)) {
        print_usage(argv[0]);
        return 1;
    }

    printf("Allocating %d MB as '%s' tier...\n", size_MB, tier);


    size_t size_bytes = (size_t)size_MB * 1024 * 1024;

if (strcmp(tier, "hot") == 0) {
    clock_t start = clock();

    // Allocate memory
    char *buffer = (char *)malloc(size_bytes);
    if (!buffer) {
        perror("malloc failed");
        return 1;
    }

    // Touch the memory (initialize)
    memset(buffer, 0, size_bytes);

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tier 0 (hot): Allocation and touch completed in %.3f seconds\n", elapsed);
    log_to_csv("hot", size_MB, elapsed);
    print_energy_consumption("hot", size_MB);
    free(buffer);  // Clean up
}

else if (strcmp(tier, "cold") == 0) {
    clock_t start = clock();

    // Allocate memory
    char *buffer = (char *)malloc(size_bytes);
    if (!buffer) {
        perror("malloc failed");
        return 1;
    }

    // Access memory with delay to simulate remote latency
    for (size_t i = 0; i < size_bytes; i += 4096) { // 4KB stride
        buffer[i] = 0;
        usleep(100);  // Introduce 100 microseconds of delay per page
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tier 1 (cold): Simulated remote allocation completed in %.3f seconds\n", elapsed);
    log_to_csv("cold", size_MB, elapsed);
    print_energy_consumption("cold", size_MB);
    free(buffer);  // Clean up
}

else if (strcmp(tier, "persistent") == 0) {
    clock_t start = clock();

    // Open file to simulate persistent memory write
    FILE *f = fopen("/tmp/persistent_data.bin", "wb");
    if (!f) {
        perror("fopen failed");
        return 1;
    }

    // Create 1MB buffer of zeroes
    char *buffer = (char *)calloc(1024 * 1024, 1);
    if (!buffer) {
        perror("calloc failed");
        fclose(f);
        return 1;
    }

    // Write repeatedly to match requested size
    for (int i = 0; i < size_MB; ++i) {
        size_t written = fwrite(buffer, 1, 1024 * 1024, f);
        if (written != 1024 * 1024) {
            perror("fwrite failed");
            free(buffer);
            fclose(f);
            return 1;
        }
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tier 2 (persistent): Write to file completed in %.3f seconds\n", elapsed);
    log_to_csv("persistent", size_MB, elapsed);
    print_energy_consumption("persistent", size_MB);
    free(buffer);
    fclose(f);
}


}
