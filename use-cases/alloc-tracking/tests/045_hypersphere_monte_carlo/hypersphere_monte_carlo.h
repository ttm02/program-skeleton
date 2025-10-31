/*
This file was taken from https://people.math.sc.edu/Burkardt/c_src/hypersphere_monte_carlo/hypersphere_monte_carlo.html
to test the analysis pass. It is NOT authored by me.
*/

double hypersphere01_area ( int m );
double hypersphere01_monomial_integral ( int m, int e[] );
double *hypersphere01_sample ( int m, int n, int *seed );
int i4vec_sum ( int n, int a[] );
double *monomial_value ( int m, int n, int e[], double x[] );
double r8_gamma ( double x );
double r8_uniform_01 ( int *seed );
double *r8mat_normal_01_new ( int m, int n, int *seed );
double *r8vec_normal_01_new ( int n, int *seed );
double r8vec_sum ( int n, double a[] );
double *r8vec_uniform_01_new ( int n, int *seed );
void timestamp ( void ); // TODO: CHANGED "void timestamp ( );""