#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 256

void loopAntiDep(int *a) {
#pragma omp parallel for firstprivate(a) schedule(static, 2)
  for (unsigned i = 1; i < N; i++) {
    a[i - 1] = a[i] + 3;
  }
}

int main() {
  int a[N];

  std::srand(std::time(0));
#pragma omp parallel for
  for (unsigned i = 0; i < N; i++) {
    a[i] = std::rand();
  }

  loopAntiDep(a);

  for (unsigned i = 0; i < N; i++) {
    printf("{%d}\t", a[i]);
  }
  return 0;
}
