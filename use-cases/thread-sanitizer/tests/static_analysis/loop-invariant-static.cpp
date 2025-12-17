#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 256

void loopInvariant(double *a, int *b) {
#pragma omp parallel for firstprivate(a, b) schedule(static, 1)
  for (unsigned i = 0; i < N; i++) {
    b[42] = a[i] / 2.718;
  }
}

int main() {
  double a[N];
  int b[N];

  std::srand(std::time(0));
#pragma omp parallel for
  for (unsigned i = 0; i < N; i++) {
    a[i] = std::rand();
    b[i] = std::rand();
  }

  loopInvariant(a, b);

  for (unsigned i = 0; i < N; i++) {
    printf("{%lf}\t", a[i]);
    printf("{%d}\t", b[i]);
  }
  return 0;
}
