#include <stdlib.h>

void* nested_nested(size_t s) {
    int ii = 15 * s;
    return malloc(s);
}

void* nested(size_t s) {
    int ii = 5 * s;
    return nested_nested(s * 2);
}

int main() {
    int* data = nested(5 * sizeof(int));
    free(data);
    
    return 0;
}
