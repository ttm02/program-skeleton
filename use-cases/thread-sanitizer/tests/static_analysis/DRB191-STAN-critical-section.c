#include <stdio.h>

int a = 100, b = 0;

int main() {
  int o = 7;
#pragma omp parallel sections shared(a, b) num_threads(2)
  {
#pragma omp section
    while (1) {
#pragma omp critical(A)
      {
        int size = b * 2;
        int count = o * 3;
        if (size < a) {
          size += 2; // produce
          printf("Produced! size=%d\n", size);
        }
        if (count < a) {
          count += 3; // produce
          printf("Produced! count=%d\n", count);
        }
        b = size / 2;
        o = count / 3;
      }
    }
#pragma omp section
    while (1) {
#pragma omp critical(B)
      {
        int size = b * 2;
        int count = o * 3;
        if (0 < size) {
          size -= 2; // consume
          printf("Consumed! size=%d\n", size);
        }
        if (0 < count) {
          count -= 3; // consume
          printf("Consumed! count=%d\n", count);
        }
        b = size / 2;
        o = count / 3;
      }
    }
  }
}
