#include <stdlib.h>


void** allocate_matrix(int rows, int cols) {
    void** matrix = malloc(rows * sizeof(void*));
    int i = 0;
    while (i < rows) {
        matrix[i] = malloc(cols * sizeof(int));
        i++;
    }

    return matrix;
}

void free_matrix(void** m, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        free(m[i]);
    }
    free(m);
}

int main() {
    int rows = 5;
    int cols = 5;

    void** m = allocate_matrix(rows, cols);
    free_matrix(m, rows, cols);

    return 0;
}
