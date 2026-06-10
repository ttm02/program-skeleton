#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <omp.h>

/*
Calculating the sequence alignment of 2 random DNA sequences using the
Smith-Waterman-Algorithm
(https://en.wikipedia.org/wiki/Smith%E2%80%93Waterman_algorithm)
*/

// score of same element:
#define _MSCORE_ 1.
// minimum function as a macro
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// compiler switches that govern what parts of the program is included
#define DO_BACKTRACK
#define PRINT_ALIGNMENT
// #define PRINT_SEQUENCES
//  a predefined sequence for testing
// #define USE_PREDEFINED_SEQUENCE

// global variables:
// Block size
int Bw, /* Block width*/ Bh; /* Block hieght*/
// length of the sequences
int N_a, N_b;

// sequences
char *seq_a;
char *seq_b;
// parameters for the algorithm
double mu, delta;
// Score-Matrix for dynamic programming
double **H;
// time measurement
struct timeval start_time, stop_time;

// Point type declaration
typedef struct {
  int row, col;
} Point;

#ifdef DO_BACKTRACK
// the Alignment
char *consensus_a;
char *consensus_b;

int tick = 0;
#endif

// Dynamic alloc functions
void free_matrix(double **);
void alloc_matrix(int, int);

// Computational - helper functions
Point movePoint(Point);
void computeBlock(int block_row, int block_col);
double find_max_score(double[]);
double similarity_score(char, char);

#ifdef USE_PREDEFINED_SEQUENCE
char *predefined_sequence(int len);
#else
// generating a random DNA Sequence
char *random_sequence(int len);
#endif

// main function that performs the algorithm
// you only need to do changes within this function
double calculate() {

  // initialize block height and block width
  // you may want to change it to larger blocks depending on the matrix size
  Bh = 16;
  Bw = 16;
  // changing the block size afterwards will break the current code

  // Calculate blocks total
  int num_Rows = (int)ceil(((double)N_a) / Bh);
  int num_Cols = (int)ceil(((double)N_b) / Bw);

// compute the score matrix
// be aware that only using a pragma omp parallel for will not be enough in
// this case!
#pragma omp parallel
  {
#pragma omp single
    {
      for (int i = 0; i < num_Rows; ++i) {
        for (int j = 0; j < num_Cols; ++j) {
#pragma omp task depend(in : H[i - 1][j], H[i - 1][j - 1], H[i][j - 1])        \
    depend(out : H[i][j])
          computeBlock(i, j);
        }
      }
    }
  }

  // no need to parallelize this as well
  // but of course feel free to do so :-)
  // Set max staring val as -Inf
  double max_val = DBL_MIN;
  int max_x = -1;
  int max_y = -1;
  // Search H for the maximal score
  for (int i = 1; i <= N_a; ++i) {
    for (int j = 1; j <= N_b; ++j) {
      if (H[i][j] > max_val) {
        max_val = H[i][j];
        max_x = i;
        max_y = j;
      }
    }
  }

  double score = max_val;
#ifdef DO_BACKTRACK
  // backtracking has to be done sequential anyway
  Point curr = {max_x, max_y}, next = movePoint(curr);

  // Backtracking from H_max
  while (((curr.row != next.row) || (curr.col != next.col)) &&
         (next.row != 0) && (next.col != 0)) {

    if (next.row == curr.row) {
      consensus_a[tick] = '-'; // deletion in A
    } else {
      consensus_a[tick] = seq_a[curr.row - 1]; // match/mismatch in A
    }

    if (next.col == curr.col) {
      consensus_b[tick] = '-'; // deletion in B
    } else {
      consensus_b[tick] = seq_b[curr.col - 1]; // match/mismatch in B
    }

    ++tick;
    curr = next;
    next = movePoint(next);
  }
#endif

  return score;
}

// main driver function thar reads arguments and initializes the data
// this it not robust to bad inputs e.g. if you specify a sequence length of -1
int main(int argc, char **argv) {

  // Input arguments check
  if (argc != 5) {
    printf("Using the default arguments (2 2 32 16)\n");
    mu = 2;
    delta = 2;
    N_a = 32;
    N_b = 16;
  } else {
    mu = atof(argv[1]);
    delta = atof(argv[2]);
    N_a = atoi(argv[3]);
    N_b = atoi(argv[4]);
  }

#ifdef USE_PREDEFINED_SEQUENCE
  seq_a = predefined_sequence(N_a);
  seq_b = predefined_sequence(N_b);
#else
  srand(time(NULL));
  seq_a = random_sequence(N_a);
  seq_b = random_sequence(N_b);
#endif
  assert(seq_a != NULL && seq_b != NULL);

#ifdef DO_BACKTRACK
  consensus_a = (char *)malloc((N_a + N_b + 2) * sizeof(char));
  consensus_b = (char *)malloc((N_a + N_b + 2) * sizeof(char));
  assert(consensus_a != NULL && consensus_b != NULL);
#endif

  printf("First sequence has length  : %6d\n", N_a);
  printf("Second sequence has length : %6d\n\n", N_b);

  // alloc score matrix
  alloc_matrix(N_a + 1, N_b + 1);
  // init score matrix
  for (int i = 0; i < N_a + 1; ++i) {
    for (int j = 0; j < N_b + 1; ++j) {
      H[i][j] = 0;
    }
  }

  gettimeofday(&start_time, NULL);
  double score = calculate();
  gettimeofday(&stop_time, NULL);

#ifdef PRINT_SEQUENCES
  printf("Sequences:\n\n");
  for (int i = 0; i < N_a; ++i) {
    printf("%c", seq_a[i]);
  }
  printf("  and\n");
  for (int i = 0; i < N_b; ++i) {
    printf("%c", seq_b[i]);
  }
  printf("\n\nparameters  mu = %.2lf and delta = %.2lf\n\n", mu, delta);
#endif

#ifdef PRINT_ALIGNMENT
#ifdef DO_BACKTRACK
  printf("\nAlignment:\n");
  for (int i = tick - 1; i >= 0; --i) {
    printf("%c", consensus_a[i]);
  }

  printf("\n");
  for (int j = tick - 1; j >= 0; --j) {
    printf("%c", consensus_b[j]);
  }
  printf("\n");
#endif
#endif

  printf("Score: %f\n", score);
  double time = (stop_time.tv_sec - start_time.tv_sec) +
                (stop_time.tv_usec - start_time.tv_usec) * 1e-6;
  printf("Calculation Time:    %f s \n", time);

#ifdef DO_BACKTRACK
  free(consensus_a);
  free(consensus_b);
#endif
  free_matrix(H);

  free(seq_a);
  free(seq_b);
  return 0;
}

/******************************************************************************/
/* Computational - helper functions */
/******************************************************************************/

/**
 * Computes the given block
 * Input Parameters are the block indices
 **/
void computeBlock(int block_row, int block_col) {

  int i, j;
  const int row = block_row * Bh, col = block_col * Bw;
  int Rows = MIN(N_a, row + Bh), Cols = MIN(N_b, col + Bw);

  double tp, tmp[3];

  for (i = row + 1; i <= Rows; ++i) {
    tp = H[i][col];
    for (j = col + 1; j <= Cols; ++j) {
      tmp[0] = H[i - 1][j - 1] + similarity_score(seq_a[i - 1], seq_b[j - 1]);
      tmp[1] = H[i - 1][j] - delta;
      tmp[2] = tp - delta;

      // Compute action that produces maximum score
      H[i][j] = tp = find_max_score(tmp);
    }
  }
}

double similarity_score(char a, char b) { return ((a == b) ? _MSCORE_ : -mu); }

double find_max_score(double values[]) {

  float max = (values[0] > values[1]) ? values[0] : values[1];
  if (values[2] > max)
    max = values[2];
  return ((max > 0.) ? max : 0.);
}

// move point during the Backtracking of the alignment
Point movePoint(Point old) {

  int row = old.row, col = old.col;

  float Val = H[row][col];

  if (Val ==
      H[row - 1][col - 1] + similarity_score(seq_a[row - 1], seq_b[col - 1])) {
    --old.row;
    --old.col;
  } else if (Val == H[row - 1][col] - delta) {
    --old.row;
  } else if (Val == H[row][col - 1] - delta) {
    --old.col;
  }
  return old;
}

/******************************************************************************/
/* auxiliary functions used by main                                           */
/******************************************************************************/

#ifdef USE_PREDEFINED_SEQUENCE
char *predefined_sequence(int len) {
  char *result = (char *)malloc(len * sizeof(char));
  const char *predef1 = "AGTGATGTGTCTCTGT";
  const char *predef2 = "AGTGATGTGTCCCTGA";
  // best matching depends on gap penalty
  // e.g. mu 2 = score 12 mu=4 score 11
  if (len == 32) {
    memcpy(result, predef1, 16);
    memcpy(result + 16, predef1, 16);
  } else if (len == 16) {
    memcpy(result, predef2, 16);
  } else {
    free(result);
    result = NULL;
  }
  return result;
}
#else
char *random_sequence(int len) {
  char *result = (char *)malloc(len * sizeof(char));
  int random;
  for (int i = 0; i < len; ++i) {
    // DNA Only consist of AGTC
    random = rand() % 4;
    switch (random) {
    case 0:
      result[i] = 'A';
      break;
    case 1:
      result[i] = 'G';
      break;
    case 2:
      result[i] = 'C';
      break;
    case 3:
      result[i] = 'T';
      break;
    default:
      result[i] = 'A';
      break;
    }
  }
  return result;
}
#endif

void alloc_matrix(int n, int m) {
  H = (double **)malloc(n * sizeof(double *));
  double *data = (double *)malloc(n * m * sizeof(double));
  assert(H != NULL && data != NULL);

  for (int i = 0; i < n; i++) {
    H[i] = &data[i * m];
  }
}

void free_matrix(double **m) {
  free(m[0]);
  free(m);
}
