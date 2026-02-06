#include <stdlib.h>
#include <stdio.h>

int main() {

    void* p = malloc(100); // 100 bytes
    free(p);

    return 0;
}
