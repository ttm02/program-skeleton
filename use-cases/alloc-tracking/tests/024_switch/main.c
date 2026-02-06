#include <stdlib.h>


void* allocate_by_type(char type, int count) {
    int ii = 3;
    
    switch (type) {
        case 'i': return malloc(count * sizeof(int));
        case 'd': return malloc(count * sizeof(double));
        default: return malloc(count);
    }
}

int main() {
    
    int* p = (int*) allocate_by_type('i', 5);
    free(p);

    return 0;
}
