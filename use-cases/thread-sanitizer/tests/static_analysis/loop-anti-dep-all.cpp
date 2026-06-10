#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 256

#define anitDepLoop(J)                                                         \
  for (unsigned i = J; i < N; i++) {                                           \
    a[i - J] = b[i] * 3.141;                                                   \
    b[i - J] = a[i] / 2.718;                                                   \
  }

void loopAntiDep(double *a, int *b, unsigned k, unsigned s) {
#pragma omp parallel for firstprivate(a, b, k) schedule(static)
  anitDepLoop(k);

#pragma omp parallel for firstprivate(a, b, k) schedule(dynamic, s)
  anitDepLoop(k);

#pragma omp parallel for firstprivate(a, b, k) schedule(guided)
  anitDepLoop(k);

#pragma omp parallel for firstprivate(a, b, k) schedule(auto)
  anitDepLoop(k);

#pragma omp parallel for firstprivate(a, b, k) schedule(runtime)
  anitDepLoop(k);
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

  loopAntiDep(a, b, 3, 5);
  loopAntiDep(a, b, 8, 8);

  for (unsigned i = 0; i < N; i++) {
    printf("{%lf}\t", a[i]);
    printf("{%d}\t", b[i]);
  }
  return 0;
}
