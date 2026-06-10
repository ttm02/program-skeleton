#include <stdio.h>

int mem = 0;

void notNeeded() { mem = 13; }
void assignMem() { mem = 42; }
void runFunc(void (*funcPtr)()) { funcPtr(); }

int main() {
  assignMem();
#pragma omp parallel
  {
    runFunc(assignMem);
  }
  notNeeded();
  printf("%d", mem);
  return 0;
}
