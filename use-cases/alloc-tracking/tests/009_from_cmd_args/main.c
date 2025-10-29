#include <stdio.h>
#include <stdlib.h>

// usage: "./009_from_cmd_args 5"
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <array_length>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int* arr = malloc(n * sizeof(int));

    free(arr);

    return 0;
}
