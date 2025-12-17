#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 256

void loopUnicorn(double *a, int *b) {
#pragma omp parallel for firstprivate(a, b) schedule(static, 5)
  for (unsigned i = 1; i < N; i++) {
    if (5 <= i) {
      a[i] = b[i] * 3.141;
    }
    if (i % 5 == 2) {
      b[i - 2] = -2;
      b[i - 1] = -1;
      b[i] = 0;
      b[i + 1] = 1;
      b[i + 2] = 2;
    }
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

  loopUnicorn(a, b);

  for (unsigned i = 0; i < N; i++) {
    printf("{%lf}\t", a[i]);
    printf("{%d}\t", b[i]);
  }
  return 0;
}
