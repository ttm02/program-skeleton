#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 256

void loopAntiDep(double *a, int *b) {
#pragma omp parallel for firstprivate(a, b) schedule(dynamic, 2)
  for (unsigned i = 1; i < N; i++) {
    a[i - 1] = b[i] * 3.141;
    b[i - 1] = a[i] / 2.718;
  }
}

void loopForwardDep(double *a, int *b) {
#pragma omp parallel for firstprivate(a, b) schedule(dynamic, 2)
  for (unsigned i = 0; i < N - 1; i++) {
    a[i] = b[i + 1] * 3.141;
    b[i] = a[i + 1] / 2.718;
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

  loopAntiDep(a, b);
  // loopForwardDep(a, b);

  for (unsigned i = 0; i < N; i++) {
    printf("{%lf}\t", a[i]);
    printf("{%d}\t", b[i]);
  }
  return 0;
}
