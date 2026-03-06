#include <stdlib.h>


char *
hypre_MAlloc( int size )
{
   char *ptr;

   if (size > 0) {
      ptr = malloc(size);
   }
   else {
      ptr = NULL;
   }

   return ptr;
}


int* wrapper_malloc(size_t s) {
    return (int*) malloc(s);
}


int main() {
    int* p = wrapper_malloc(5 * sizeof(int));
    free(p);
    
    char* cp = hypre_MAlloc(5);
    free(cp);

    return 0;
}
