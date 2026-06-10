#include <cstdlib>
#include <ctime>

#define N 8

void loopInvariant(const int x, const int y, double *u, double *p, double *r,
                   const double *d, const double *e) {
#pragma omp parallel for
  for (int jj = 0; jj < y; ++jj) {
    for (int kk = 0; kk < x; ++kk) {
      const int index = kk + jj * x;
      p[0] = 0.0;
      r[index] = std::rand();
      u[index] = e[index] * d[index];
    }
  }
}

int main() {
  double u[N], p[N], r[N];
  double d[N], e[N];

  std::srand(std::time(0));

  for (int i = 1; i < N; i++)
    for (int j = 1; j < N; j++)
      loopInvariant(i, j, u, p, r, d, e);

  return 0;
}
