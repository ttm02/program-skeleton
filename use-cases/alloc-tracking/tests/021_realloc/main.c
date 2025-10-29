#include <stdlib.h>



int main() {
    size_t old_size = 5 * sizeof(int);
    int* p = (int*)malloc(old_size); 

    int* pp = realloc(p, old_size * 2);
    free(pp);

    // ==> Total allocation size: new_size
    // Note: does not matter if smaller or larger
    //       However, need to know which pointer as parameter 
    //       to find original malloc call and remove it from total memory?   

    return 0;
}
