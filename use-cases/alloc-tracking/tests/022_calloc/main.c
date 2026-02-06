#include <stdlib.h>


int main() {
    float* p = (float*) calloc(5, sizeof(float));
    free(p);
    
    return 0;
}
