#include <assert.h>
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STATESIZE 64

// returns random float in range -1 to 1
double rand_dbl(struct random_data *random_buf) {
  int rand_int;
  random_r(random_buf, &rand_int);
  return (double)rand_int / RAND_MAX * 2.0 - 1.0;
}

int main(int argc, char *argv[]) {
  // reading the number of points to use from the argument
  unsigned long num_points = 100000;
  if (argc > 1) {
    num_points = atol(argv[1]);
  }
  double n = 0; // inner circle counter

// threads should use a different random seed
#pragma omp parallel
  {
    // be aware that each thread should have its own random state
    struct random_data *random_buf = calloc(1, sizeof(struct random_data));
    char *state_buf = calloc(STATESIZE, sizeof(char));
    // to be clear: random_data and state_buf are both part of the random state
    assert(random_buf != NULL && state_buf != NULL);

    unsigned int seed = time(NULL);
#ifdef OMP
    seed *= (omp_get_thread_num() + 1);
#endif
    initstate_r(seed, state_buf, STATESIZE, random_buf);

#pragma omp for reduction(+ : n)
    for (unsigned long i = 0; i < num_points; i++) {
      double x = rand_dbl(random_buf);
      double y = rand_dbl(random_buf);
      if (sqrt(x * x + y * y) < 1.0) {
        n++;
      }
    }

    free(random_buf);
    free(state_buf);
  }

  double pi = n / num_points * 4;
  printf("Pi is approx. %f\n", pi);
  return 0;
}
