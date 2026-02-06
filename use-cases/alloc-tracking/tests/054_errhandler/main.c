#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

/* Custom MPI error handler */
void check_for_mpi_error(MPI_Comm comm, int *errcode, ...) {
    char errstr[MPI_MAX_ERROR_STRING];
    int len = 0;

    MPI_Error_string(*errcode, errstr, &len);
    fprintf(stderr, "MPI error occurred: %s\n", errstr);

    MPI_Abort(comm, *errcode);
}

#define MPI_CHECK(call)                                  \
    do {                                                  \
        int err = (call);                                \
        if (err != MPI_SUCCESS) {                         \
            check_for_mpi_error(MPI_COMM_WORLD, &err);    \
        }                                                 \
    } while (0)

int main(int argc, char **argv) {
    MPI_Comm err_comm;
    MPI_Errhandler errhandler;
    int rank, size;

    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &size));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

  MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr, "This program requires at least 2 MPI processes\n");
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (rank == 1) {
        int requested_elems = 1024;  /* arbitrary test value */

        MPI_CHECK(
            MPI_Send(&requested_elems,
                     1,
                     MPI_INT,
                     0,
                     0,
                     MPI_COMM_WORLD)
        );
    }

    if (rank == 0) {
        int requested_elems = 0;
        int *buffer = NULL;

        MPI_Status status;

        MPI_CHECK(
            MPI_Recv(&requested_elems,
                     1,
                     MPI_INT,
                     1,
                     0,
                     MPI_COMM_WORLD,
                     &status)
        );

        if (requested_elems <= 0) {
            fprintf(stderr, "Invalid allocation size received: %d\n",
                    requested_elems);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        buffer = malloc(requested_elems * sizeof(int));
        if (!buffer) {
            fprintf(stderr, "Memory allocation failed on rank 0\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        /* Touch memory to make allocation observable */
        for (int i = 0; i < requested_elems; i++) {
            buffer[i] = requested_elems;
        }

        printf("Rank 0 allocated and initialized %d integers\n",
               buffer[0] );

        free(buffer);
    }

    MPI_CHECK(MPI_Finalize());

    return 0;
}
