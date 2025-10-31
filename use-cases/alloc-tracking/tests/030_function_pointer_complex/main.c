#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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


typedef struct
{
   int    (*precond_setup)();

} test_funcs;


typedef struct
{
   void    *precond_data;
   test_funcs * functions;

} test_data;


int
testF( void *vdata,
                  void *A,
                  void *b,
                  void *x         )
{
   test_data *data     = vdata;
   test_funcs *funcs = data->functions;

   int          (*precond_setup)() = (funcs->precond_setup);
   void          *precond_data     = (data -> precond_data);
   int            ierr = 0;
 
   ierr = precond_setup(precond_data, A, b, x);

   int* pp = (int*) malloc(sizeof(int) + ierr + sizeof(int));
 
   return ierr;
}

int example_precond_setup(void *precond_data, void *A, void *b, void *x)
{
    printf("Preconditioner setup called!\n");
    printf("precond_data: %p\n", precond_data);
    printf("A: %p, b: %p, x: %p\n", A, b, x);
    int i = 0;
    for (int ii = 0; ii < 10; ii++) {
        i += ii;
    }
    return i; // success
}


int main(int argc, char **argv) {
 
    int dummy_A, dummy_b, dummy_x;
    int dummy_precond_data = 42;
    
    test_funcs funcs;
    funcs.precond_setup = example_precond_setup;
    
    test_data data;
    data.precond_data = &dummy_precond_data;
    data.functions = &funcs;
    
    printf("Calling testF...\n");
    int result = testF(&data, &dummy_A, &dummy_b, &dummy_x);
    
    printf("Function returned: %d\n", result);



    /* This does not work since a std function is behind a function pointer
        that is afterwards used to allocate memory.

    const char *num_str = "42";
    int (*func_ptr2)(const char *);
    func_ptr2 = atoi;
    int size = func_ptr2(num_str);
    printf("size: %d\n", size);


    char* pp = (char*)malloc(size + sizeof(char));
    free(pp);
    */

    // The following example works however since the length is not used
    // to allocate memory and it will be simply removed from the slice.
    const char *str = "Hello";
    size_t length = strlen(str);  // Returns 5
    printf("Length: %zu\n", length);

    size_t (*func_ptr)(const char *, size_t);
    func_ptr = strnlen;
    length = func_ptr(str, 100);
    printf("Length: %zu\n", length);


    // Simply allocate memory based on std func return value.
    const char *str2 = "Hello";
    size_t length2 = strlen(str2);  // Returns 5  

    char* pp4 = hypre_MAlloc(length2 + sizeof(char));
    free(pp4);


    char* pp2 = (char*)malloc(length2 + sizeof(char));
    free(pp2);


    char* pp3 = hypre_MAlloc(length2 + sizeof(char));
    free(pp3);


    return 0;
}