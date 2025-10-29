#include <stdlib.h>

int main() {

    int* p = (int*)malloc(sizeof(int));
    free(p);
    
    return 0;
}
