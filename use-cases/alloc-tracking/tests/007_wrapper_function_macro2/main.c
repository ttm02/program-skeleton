#include <stdlib.h>

void* cooler_malloc(size_t s) {
    return malloc(s);
}

#define wrap_malloc(type, count) \
( (type *)cooler_malloc((unsigned int)(sizeof(type) * (count))) )


int main() {

    int* p = wrap_malloc(int, 5);
    free(p);


    int* p2 = wrap_malloc(int, 15);
    free(p2);

    int* p3 = wrap_malloc(int, 115);
    free(p3);
    
    return 0;
}
