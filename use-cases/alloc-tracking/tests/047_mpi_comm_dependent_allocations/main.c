#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define MA_MAIN
#include "block.h"


void *ma_malloc(size_t size, char *file, int line)
{
   void *ptr;

   ptr = (void *) malloc(size);

   if (ptr == NULL) {
      printf("NULL pointer from malloc call in %s at %d\n", file, line);
      exit(-1);
   }

   return(ptr);
}


void allocate(void)
{
    size_t recv_size;

    if (my_pe == 0) {
        recv_size = 9999;
        MPI_Send(&recv_size, 1, MPI_UNSIGNED_LONG, 1, 1, MPI_COMM_WORLD);


    } else if (my_pe == 1) {
        MPI_Recv(&recv_size, 1, MPI_UNSIGNED_LONG, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int i, j, k, m, n;

        num_blocks = (num_sz *) ma_malloc((num_refine+1)*sizeof(num_sz), __FILE__, __LINE__);
        num_blocks[0] = num_pes*init_block_x*init_block_y*init_block_z;
        local_num_blocks = (num_sz *) ma_malloc((num_refine+1)*sizeof(num_sz), __FILE__, __LINE__);
        local_num_blocks[0] = init_block_x*init_block_y*init_block_z;

        blocks = (block *) ma_malloc(max_num_blocks*sizeof(block),  __FILE__, __LINE__);

        for (n = 0; n < max_num_blocks; n++) {
            blocks[n].number = -1;
            blocks[n].array = (double ****) ma_malloc(num_vars*sizeof(double ***),  __FILE__, __LINE__);

            for (m = 0; m < num_vars; m++) {
                blocks[n].array[m] = (double ***) ma_malloc((x_block_size+2)*sizeof(double **), __FILE__, __LINE__);

                for (i = 0; i < x_block_size+2; i++) {
                    blocks[n].array[m][i] = (double **) ma_malloc((y_block_size+2)*sizeof(double *), __FILE__, __LINE__);

                    for (j = 0; j < y_block_size+2; j++) {
                        // note: this leaks, just testing
                        double* dd = (double *) ma_malloc((recv_size+2)*sizeof(double), __FILE__, __LINE__);
                        blocks[n].array[m][i][j] = (double *) ma_malloc((z_block_size+2)*sizeof(double), __FILE__, __LINE__);
                    }
                }
            }
        }
    }
}

void deallocate(void)
{
    if (my_pe == 1) {
        int i, j, m, n;

        for (n = 0; n < max_num_blocks; n++) {
            for (m = 0; m < num_vars; m++) {
                for (i = 0; i < x_block_size+2; i++) {
                    for (j = 0; j < y_block_size+2; j++)
                    free(blocks[n].array[m][i][j]);
                    free(blocks[n].array[m][i]);
                }
                free(blocks[n].array[m]);
            }
            free(blocks[n].array);
        }
        free(blocks);
    }
}

int important_calculation() {
    int c = 0;

    for (int i = 0; i < 100; ++i) {
        c++;
    }

    return c;
}

int main(int argc, char **argv)
{
    int ierr;
#include "param.h"
    ierr = MPI_Init(&argc, &argv);
    ierr = MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_ARE_FATAL);
    ierr = MPI_Comm_rank(MPI_COMM_WORLD, &my_pe);
    ierr = MPI_Comm_size(MPI_COMM_WORLD, &num_pes);


    max_num_blocks = 100;
    num_refine = 2;
    num_vars = 5;

    x_block_size = 4;
    y_block_size = 4;
    z_block_size = 4;

    init_block_x = 1;
    init_block_y = 1;
    init_block_z = 1;

    allocate();

    int res = important_calculation();

    deallocate();

    MPI_Finalize();

    return 0;
}
