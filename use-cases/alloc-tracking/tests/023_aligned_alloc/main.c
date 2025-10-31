#include <stdlib.h>


int main() {
    size_t alignment = 64;  // 64-byte alignment (typical cache line size)
    size_t size = 256;      // Allocate 256 bytes
    
    // Alignment must be a power of 2 and size must be a multiple of alignment
    void* ptr = aligned_alloc(alignment, size);
    free(ptr);
    
    return 0;
}
