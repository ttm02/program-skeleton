/******************  su3_adjoint.c  (in su3.a) **************************
*									*
* void su3_adjoint( su3_matrix *a, su3_matrix *b )			*
* B  <- A_adjoint,  adjoint of an SU3 matrix 				*
*/
#include "config.h"
#include "complex.h"
#include "su3.h"


void su3_adjoint( su3_matrix *a, su3_matrix *b ) __attribute__ ((no_instrument_function));

/* adjoint of an SU3 matrix */
void su3_adjoint( su3_matrix *a, su3_matrix *b ){
register int i,j;
    for(i=0;i<3;i++)for(j=0;j<3;j++){
	CONJG( a->e[j][i], b->e[i][j] );
    }
}
