int main() {
  int a = 5;
#pragma omp parallel
  {
#pragma omp single
    {
      a = 3;
    }
#pragma omp single
    {
      a = 7;
    }
  }
  return a;
}
