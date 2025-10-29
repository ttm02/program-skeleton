#include <stdlib.h>


#ifdef DEBUG
#define ALLOC_FUNC(x) malloc(x + 8)
#else
#define ALLOC_FUNC(x) malloc(x)
#endif

// Note defines do not matter really since it simply replaces
// the ALLOC_FUNC by correct malloc call. Not visible in the IR.

int main() {
    size_t size = sizeof(int) * 5;

    void* p = ALLOC_FUNC(size);

    return 0;
}
