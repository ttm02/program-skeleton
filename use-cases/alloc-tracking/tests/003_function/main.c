#include <stdlib.h>

void func() {
    int* p = (int*) malloc(5 * sizeof(int));
    free(p);
}

int main() {

    func();
    
    return 0;
}
