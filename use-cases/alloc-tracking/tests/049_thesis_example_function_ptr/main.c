#include <stdio.h>
#include <stdlib.h>

struct Data {
    size_t (*fn)(size_t);
};

size_t add_one(size_t x) {
    return x + 1;
}

int main() {
    struct Data d;

    // NOTE: Analysis works only when function pointer is set to concrete value.
    //       Commenting out the following line will not work, in debug it triggers assert "cannot find call target"
    d.fn = add_one;

    if (d.fn) {
        size_t result = d.fn(5);
        double* p = (double*)malloc(sizeof(double) * result);
    }
    
    return 0;
}
