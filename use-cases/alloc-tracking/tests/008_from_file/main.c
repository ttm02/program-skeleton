#include <stdio.h>
#include <stdlib.h>


int main() {
    FILE* f = fopen("../../../use_cases/test_suite/tests/008_from_file/input.txt", "r");
    int n;
    fscanf(f, "%d", &n);
    int* arr = malloc(n * sizeof(int));
    fclose(f);
    free(arr);

    return 0;
}
