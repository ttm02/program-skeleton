/*
 * Testcase inspired by "Figure 1" in the "BIG FOOT: Static Check Placement for
 * Dynamic Race Detection" paper.
 * https://doi.org/10.1145/3062341.3062350
 */

#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 65536

struct Point {
  int x, y, z;

  void move(const int dx, const int dy, const int dz) {
    x += dx;
    y += dy;
    z += dz;
  }
};

void movePts(Point *a, const unsigned lo, const unsigned hi) {
#pragma omp parallel for firstprivate(a, lo, hi)
  for (unsigned i = 5; i < 20; i++)
    a[i].move(1, 1, 1);
}

int main() {
  std::srand(std::time(0));
  Point a[N];

#pragma omp parallel for
  for (unsigned i = 0; i < N; i++) {
    a[i].x = std::rand();
    a[i].y = std::rand();
    a[i].z = std::rand();
  }

  unsigned lo = std::rand() % (N);
  unsigned hi = std::rand() % (N);
  movePts(a, lo, hi);

  for (unsigned i = 0; i < N; i++) {
    if (i % 50)
      printf("\n");
    printf("[%d, %d, %d]\t", a[i].x, a[i].y, a[i].z);
  }
  return 0;
}
