#include <stdlib.h>

struct complex_data {
    double values[3];
    int metadata[5];
};


int main() {
    struct complex_data* data = (struct complex_data*)malloc(sizeof(struct complex_data));
    free(data);

    return 0;
}
