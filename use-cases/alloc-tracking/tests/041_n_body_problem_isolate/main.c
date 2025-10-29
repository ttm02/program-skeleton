#include <stdio.h>
#include <stdlib.h>
 

typedef struct {
    int nstep;
    int alloc;
} RKMethod;

void Hiroshi912(RKMethod *rkm) {
    rkm->nstep = 29;

    double* c = (double *)malloc(rkm->nstep * sizeof(double));
}

RKMethod RKMethodSelection() {
    RKMethod rkm;
	rkm.alloc = 0;
    Hiroshi912(&rkm);

    return rkm;
}

int main(void) {
    int n = 10;

    RKMethod rkm = RKMethodSelection();

    double* f = (double *)malloc((n * rkm.nstep) * sizeof(double));

    return 0;
}
