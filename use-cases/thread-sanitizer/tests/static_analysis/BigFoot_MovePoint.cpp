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
  int x;
  long y;
  int z;

  inline void move(const int dx, const int dy, const int dz) {
    x += dx;
    y += dy;
    z += dz;
  }
  inline void moveXY(const int dx, const int dy) {
    x += dx;
    y += dy;
  }
  inline void moveYZ(const int dy, const int dz) {
    y += dy;
    z += dz;
  }
  inline void moveXZ(const int dx, const int dz) {
    x += dx;
    z += dz;
  }
};

static inline void moveWithBasePointer(Point *a) {
  auto j = 1;
  auto m = std::rand() % 20;

  a[0].move(1, 1, 1);
  a[j].move(1, 1, 1);
#pragma omp critical(BP)
  {
  }
  a[j + 1].move(1, 1, 1);

  a[0 + 20].moveYZ(2, 2);
  a[0 + 21].moveXY(2, 2);

  a[0 + 23].moveXZ(3, 3);

  a[0 + m].moveXZ(3, 3);
}

static inline void moveWithOffset(Point *a, unsigned i) {
  auto j = i + 1;
  auto m = std::rand() % 20;

  a[i].move(1, 1, 1);
  a[j].move(1, 1, 1);
  a[j + 1].move(1, 1, 1);

  a[i + 20].moveYZ(2, 2);
#pragma omp critical(O)
  {
  }
  a[i + 21].moveXY(2, 2);

  a[i + 23].moveXZ(3, 3);

  a[i + m].moveXZ(3, 3);
}

void movePtsConst(Point *a) {
#pragma omp parallel for firstprivate(a)
  for (unsigned i = 5; i < 5 + 15; i += 3) {
    moveWithOffset(a, i);
    moveWithBasePointer(&a[i + 50]);
  }
}

void movePts(Point *a, const unsigned lo) {
#pragma omp parallel for firstprivate(a, lo)
  for (unsigned i = lo; i < lo + 15; i += 3) {
    moveWithOffset(a, i);
    moveWithBasePointer(&a[i + 50]);
  }
}

void changeB(double *b, int lo, int hi) {
#pragma omp parallel for firstprivate(b, lo, hi)
  for (int i = lo; i < hi; i += 4) {
    double tmp;
    tmp = b[i + 0];
    b[i + 0] = tmp + 1;
    tmp = b[i + 1];
    b[i + 1] = tmp + 1;
    tmp = b[i + 2];
    b[i + 2] = tmp + 1;
    tmp = b[i + 3];
    b[i + 3] = tmp + 1;
  }
}

int main() {
  std::srand(std::time(0));
  Point a[N];
  double b[N];

#pragma omp parallel for
  for (unsigned i = 0; i < N; i++) {
    a[i].x = std::rand();
    a[i].y = std::rand();
    a[i].z = std::rand();
    b[i] = std::rand();
  }

#pragma omp parallel
#pragma omp single
  {
    unsigned o = 0;
    b[o] = std::rand();
    b[o + 1] = std::rand();
    b[o + 5] = std::rand();
    b[o + 6] = std::rand();
  }

  movePtsConst(a);
  unsigned lo = std::rand() % (N);
  unsigned hi = std::rand() % (N);
  movePts(a, (lo - 100) % (N));
  changeB(b, lo, hi - 4);

  for (unsigned i = 0; i < N; i++) {
    if (i % 50)
      printf("\n");
    printf("[%d, %ld, %d]\t", a[i].x, a[i].y, a[i].z);
    printf("{%lf}\t", b[i]);
  }
  return 0;
}
