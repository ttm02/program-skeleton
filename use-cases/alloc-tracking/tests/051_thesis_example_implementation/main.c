#include <stdio.h>
#include <stdlib.h>

int init() {
    return 100;
}

void heavy_computation(int* p) {
    for (int i = 0; i < 100; ++i) {
        p[i] = i;
    }
}


int main() {
  int n = init();
    
  printf("n is %d\n", n);
    
  int* p = (int*)malloc(n * sizeof(int));
    
  heavy_computation(p);
    
  free(p);
    
  return 0;
}
