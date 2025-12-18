#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 256

void loopUnicorn(double *a, double *b) {
#pragma omp parallel for firstprivate(a, b) schedule(static, 5)
  for (long unsigned i = 0; i < N; i++) {
    if (5 <= i) {
      a[i] = b[i] * 3.141;
    }
    if (i % 5 == 0) {
      b[i] = 0;
      b[i + 1] = 1;
      b[i + 2] = 2;
      b[i + 3] = 3;
      b[i + 4] = 4;
    }
  }
}

int main() {
  double a[N];
  double b[N];

  std::srand(std::time(0));
#pragma omp parallel for
  for (unsigned i = 0; i < N; i++) {
    a[i] = std::rand();
    b[i] = std::rand();
  }

  loopUnicorn(a, b);

  for (unsigned i = 0; i < N; i++) {
    printf("{%lf}\t", a[i]);
    printf("{%d}\t", b[i]);
  }
  return 0;
}
