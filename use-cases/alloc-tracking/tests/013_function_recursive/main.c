#include <stdlib.h>

void func(int number) {
    int* p = (int*) malloc(5 * sizeof(int));
    free(p);

    number = number - 1;

    if (number != 0) {
        func(number);
    }
}

int main() {

    int number = 3;
    func(number);
    
    return 0;
}
