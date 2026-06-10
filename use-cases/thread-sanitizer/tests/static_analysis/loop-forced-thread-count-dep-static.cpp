#include <cstdlib>
#include <ctime>
#include <omp.h>
#include <stdio.h>

#define N 256
#define D 4

void loopAntiDep(int *a) {
  omp_set_num_threads(2);
#pragma omp parallel for firstprivate(a) schedule(static, 2)
  for (unsigned i = D; i < N; i++) {
    a[i - D] = a[i];
  }
}

void loopForwardDep(int *a) {
  omp_set_num_threads(2);
#pragma omp parallel for firstprivate(a) schedule(static, 2)
  for (unsigned i = D; i < N; i++) {
    a[i] = a[i - D];
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
