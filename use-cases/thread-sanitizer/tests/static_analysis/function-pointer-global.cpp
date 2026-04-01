/*
 * Without slicing this works, but precompute currently does not replace the
 * stores similar to calls.
 */

#include <stdio.h>

int mem = 0;
void (*funcPtr)() = nullptr;

void notNeeded() { mem = 13; }
void assignMem() { mem = 42; }
void assignFunc() { funcPtr = assignMem; }
void runFunc() { funcPtr(); }

int main() {
  assignMem();
  assignFunc();
#pragma omp parallel
  {
    runFunc();
  }
  notNeeded();
  printf("%d", mem);
  return 0;
}
