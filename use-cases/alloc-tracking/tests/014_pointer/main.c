#include <stdlib.h>


int main() {
    int n = 5;
    int* np = &n;

    int* p = (int*) malloc((*np) * sizeof(int));
    free(p);
    
    return 0;
}
