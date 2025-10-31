#include <stdlib.h>

void* (*allocator)(size_t);

void setup_allocator() {
    allocator = malloc;
}

int main() {
    setup_allocator();

    void* data = allocator(1024); // NOTE: This will not be tracked since malloc behind func ptr
    free(data);

    char* t = (char*) malloc(5);
    free(t);
    
    return 0;
}
