#include <stdlib.h>

#define wrap_malloc(type, count) \
( (type *)malloc((unsigned int)(sizeof(type) * (count))) )


int main() {

    int* p = wrap_malloc(int, 5);
    free(p);
    
    return 0;
}
