#include <stdlib.h>


int main() {
    int n = 5;

    int* int_array = (int*) malloc(n * sizeof(int));
    int* int_array2 = (int*) malloc(n * sizeof(int));
    int* zero_array = (int*) calloc(n, sizeof(int));
    int* resized_array = (int*) realloc(int_array, n*2 * sizeof(int));
    void* aligned_memory = aligned_alloc(64, 256);


    free(resized_array); // free(int_array) implicit
    free(int_array2);
    free(zero_array);
    free(aligned_memory);
    
    return 0;
}
