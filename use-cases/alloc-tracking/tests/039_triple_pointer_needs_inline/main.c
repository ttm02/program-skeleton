#include <stdio.h>
#include <stdlib.h>
 
typedef struct {
  size_t nx;
  size_t ny;
} domain_t;

typedef struct node_t_ {
  void * ptr;
  struct node_t_ * next;
} node_t;

// memory pool storing all pointers to the allocated buffers in this library
static node_t * memory = NULL;

static void * memory_alloc(
    const size_t count,
    const size_t size
){
  void * ptr = malloc(count * size);
  node_t * node = malloc(1 * sizeof(node_t));
  node->ptr = ptr;
  node->next = memory;
  memory = node;
  return ptr;
}

int prepare_array(
    const size_t nitems[2],
    double *** const array
) {
  double * const buffer = memory_alloc(nitems[0] * nitems[1], sizeof(double));
  *array = memory_alloc(nitems[0], sizeof(double *));
  for (size_t i = 0; i < nitems[0]; i++) {
    (*array)[i] = buffer + nitems[1] * i;
  }
  return 0;
}


int main(void) {

    double ** array = NULL;
    const size_t nitems[2] = { 66, 66 };

    prepare_array(nitems, &array);
    
    array[5][5] = 999.;
    /*for (size_t j = 1; j <= 64; j++) {
        for (size_t i = 1; i <= 64; i++) {
            array[j][i] = 999.;
        }
    }*/

    int ii = (int)array[5][5];
    double* pp = (double*) malloc(ii * sizeof(double));

    return 0;
}
