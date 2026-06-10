#define N 8

void loopInvariant(const int x, const int y, double *p) {
#pragma omp parallel for
  for (int jj = 0; jj < y; ++jj) {
    for (int kk = 0; kk < x; ++kk) {
      p[0] = 0.0;
    }
  }
}

int main() {
  double p[N];

  for (int i = 1; i < N; i++)
    for (int j = 1; j < N; j++)
      loopInvariant(i, j, p);

  return 0;
}
