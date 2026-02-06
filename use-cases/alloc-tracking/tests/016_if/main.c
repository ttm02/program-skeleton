#include <stdlib.h>


int get_condition() {
    return 0;
}

int main() {
    int condition = get_condition();
    int* p;

    if (condition) {
        p = (int*) malloc(5 * sizeof(int));
        free(p);
        p = NULL;
    } else {
        p = (int*) malloc(500 * sizeof(int));
        free(p);
        p = NULL;
    }

    return 0;
}
