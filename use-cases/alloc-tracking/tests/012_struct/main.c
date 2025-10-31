#include <stdlib.h>


struct complex_data {
    int length;
};

int main() {
    struct complex_data cd = { .length = 5 };

    int* p = (int*) malloc(cd.length * sizeof(int));
    free(p);

    return 0;
}
