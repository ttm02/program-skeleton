/*
 * Testcase inspired by "Figure 1" in the "BIG FOOT: Static Check Placement for
 * Dynamic Race Detection" paper.
 * https://doi.org/10.1145/3062341.3062350
 */

#include <cstdlib>
#include <ctime>
#include <stdio.h>

#define N 65536

struct MyData1 {
  int a;
  short b;
  char c;
};

struct MyData2 {
  int a;
  long b;
  float c;
};

struct MyContainer {
  MyData1 p;
  MyData2 q;

  inline void move(const int dx, const int dy, const int dz) {
    p.a += dx;
    p.b += dy;
    p.c += dz;
    q.a += dx;
    q.b += dy;
    q.c += dz;
  }
  inline void moveXY(const int dx, const int dy) {
    p.a += dx;
    p.b += dy;
  }
  inline void moveYZ(const int dy, const int dz) {
    q.b += dy;
    q.c += dz;
  }
  inline void moveXZ(const int dx, const int dz) {
    p.a += dx;
    p.b += dz;
    q.b += dx;
    q.c += dz;
  }
};

static inline void moveWithBasePointer(MyContainer *a) {
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

static inline void moveWithOffset(MyContainer *a, unsigned i) {
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

void movePtsConst(MyContainer *a) {
#pragma omp parallel for firstprivate(a)
  for (unsigned i = 5; i < 5 + 15; i += 3) {
    moveWithOffset(a, i);
    moveWithBasePointer(&a[i + 50]);
  }
}

void movePts(MyContainer *a, const unsigned lo) {
#pragma omp parallel for firstprivate(a, lo)
  for (unsigned i = lo; i < lo + 15; i += 3) {
    moveWithOffset(a, i);
    moveWithBasePointer(&a[i + 50]);
  }
}

int main() {
  std::srand(std::time(0));
  MyContainer a[N];

#pragma omp parallel for
  for (unsigned i = 0; i < N; i++) {
    a[i].p.a = std::rand();
    a[i].p.b = std::rand();
    a[i].p.c = std::rand();
    a[i].q.a = std::rand();
    a[i].q.b = std::rand();
    a[i].q.c = std::rand();
  }

  movePtsConst(a);
  unsigned lo = std::rand() % (N);
  movePts(a, (lo - 100) % (N));

  for (unsigned i = 0; i < N; i++) {
    printf("[%d, %d, %d]\t", a[i].p.a, a[i].p.b, a[i].p.c);
    printf("[%d, %ld, %f]\t", a[i].q.a, a[i].q.b, a[i].q.c);
  }
  return 0;
}
