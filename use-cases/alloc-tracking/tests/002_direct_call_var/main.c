#include <stdlib.h>
#include <stdio.h>

int main() {

    int i = 4 + 3;
    int ii = i + 1;
    printf("i is %d", i);
    size_t s = 5 * sizeof(int);
    int* p = (int*) malloc(s);

    *p = 4;

    free(p);
    
    return 0;
}
