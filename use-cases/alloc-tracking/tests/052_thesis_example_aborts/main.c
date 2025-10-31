#include <stdio.h>
#include <stdlib.h>

int init() {
    return 0;
}

int check_and_increment_n(int n) {
  if (n == 0) exit(-1);
    
  return n;
}

// THIS EXAMPLE SHOWS A BROKEN PROGRAM. A DIFFERENT BEHAVIOR OF THE SLICE
// COMPARED TO THE ORIGINAL PROGRAM.
// ORIGINAL: EXIT WITH CODE -1
// SLICE: CONTINUE EXECUTION AND CRASH DUE TO DIVISION BY ZERO
int main() {
  int n = init();
  printf("n is %d\n", n);

  n = check_and_increment_n(n);
  
  // add a division by n and a malloc call that uses the result to allocate memory
  int divisor = 100;
  int result = divisor / n;
  printf("Division result: %d / %d = %d\n", divisor, n, result);

  // Allocate memory based on the division result
  char *p = (char*)malloc(result * sizeof(char));

  free(p);
    
  return 0;
}
