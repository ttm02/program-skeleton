#include <stdlib.h>

size_t get_required_size(int param) {
    return (param * param + 10) * sizeof(int);
}

int main() {
    size_t s = get_required_size(2);
    int* p = (int*) malloc(s);
    free(p);
    
    return 0;
}
