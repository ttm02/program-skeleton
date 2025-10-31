#include <stdlib.h>


int main() {
    void* p = malloc(0);
    free(p);
    
    return 0;
}
