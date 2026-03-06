#include <stdio.h>
#include <stdlib.h>
 

struct Test_arr {
    size_t count;
    size_t capacity;
    size_t (*grow_fn)(size_t count, size_t capacity);
    double* p;
};

static inline size_t Test_arr_add(struct Test_arr *a) {
    if (a->count >= a->capacity) {
        size_t new_capacity = a->grow_fn ? a->grow_fn(a->count, a->capacity) : 0;
        a->p = malloc(sizeof(double) * new_capacity);
        a->capacity = new_capacity;
    }

    return a->count++;
}

size_t f1(size_t count, size_t capacity) {
    return count + capacity;
}

size_t f2(size_t count, size_t capacity) {
    return count - capacity;
}

int main() {
    struct Test_arr* t = malloc(sizeof(*t));
    t->capacity = 2;
    t->count = 3;
    t->grow_fn = f1; // NOTE: If this is commented out the slicing analysis fails

    Test_arr_add(t);
 
    return 0;
}