#include <stdlib.h>

void func(size_t s) {
    int* p = (int*) malloc(s);
    free(p);
}

int main() {
    size_t s = 5 * sizeof(int);
    func(s);
    
    return 0;
}
