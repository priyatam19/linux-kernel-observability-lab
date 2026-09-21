#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <errno.h>
#include <string.h>

#define SYS_S2_ENCRYPT 548  // Ensure this matches your syscall number

void print_usage(const char *prog_name) {
    printf("Usage: %s -s <string> -k <key>\n", prog_name);
}

int main(int argc, char *argv[]) {
    char *input_str = NULL;
    int key = -1;
    int opt;

    // Parse command-line arguments using getopt
    while ((opt = getopt(argc, argv, "s:k:")) != -1) {
        switch (opt) {
            case 's':
                input_str = optarg;
                break;
            case 'k':
                key = atoi(optarg);
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    // Check if both -s and -k options were provided
    if (!input_str || key == -1) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    // Call the system call
    int ret = syscall(SYS_S2_ENCRYPT, input_str, key);

    if (ret == 0) {
        printf("System call executed successfully.\n");
    } else {
        printf("System call failed: %s\n", strerror(errno));
    }

    return ret;
}
