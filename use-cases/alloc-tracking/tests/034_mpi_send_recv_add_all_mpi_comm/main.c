#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



char *
hypre_MAlloc( int size )
{
   char *ptr;

   if (size > 0) {
      ptr = malloc(size);
   }
   else {
      ptr = NULL;
   }

   return ptr;
}


int main(int argc, char **argv) {
    int i = 99;
    int ierr = 3;
    char* pp = (char*)malloc(sizeof(char) + ierr + sizeof(char));
    printf("pp from malloc: %p\n", pp);
    free(pp);
    
    MPI_Init(&argc, &argv);
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    if (world_size < 2) {
        fprintf(stderr,"Requires at least 2 processes.\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    size_t recv_size;
    char* recv_buffer = NULL;
    
    if (rank == 0) {
        // Rank 0 determines size and sends it
        recv_size = 50; // Example size
        printf("Rank 0: Sending size %zu to Rank 1.\n", recv_size);
        MPI_Send(&recv_size, 1, MPI_UNSIGNED_LONG, 1, 1, MPI_COMM_WORLD);
         // Could also send data here...
    } else if (rank == 1) {
        //recv_size = 50; // Example size
        // Rank 1 receives size, then allocates
        MPI_Recv(&recv_size, 1, MPI_UNSIGNED_LONG, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank 1: Received size %zu from Rank 0.\n", recv_size);
        
        // Allocate buffer based on received size
        recv_buffer = (char*)malloc(recv_size * sizeof(char)); // hypre_MAlloc(recv_size * sizeof(char));
        printf("recv_buffer from malloc: %p\n", recv_buffer);
        if (!recv_buffer) {
            fprintf(stderr, "Rank 1: Failed to allocate receive buffer.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        int i;
        if (rank == 1) {
            i = 0;
        } else {
            i = 1;
        }
        printf("Rank 1: Allocated receive buffer (%zu bytes).\n", recv_size * sizeof(char));
        free(recv_buffer);
        
        int* p = (int*)calloc(sizeof(double), recv_size + i);
        printf("p from calloc: %p\n", p);
        free(p);
        
        int ierr3 = 3;
        char* pp3 = hypre_MAlloc(sizeof(char) + ierr3 + sizeof(char));
        printf("pp3 from hypre_MAlloc: %p\n", pp3);
        free(pp3);
    }
    
    int ierr2 = 3;
    char* pp2 = hypre_MAlloc(sizeof(char) + ierr2 + sizeof(char));
    printf("pp2 from hypre_MAlloc: %p\n", pp2);
    free(pp2);
    
    // some additional mpi send and recv calls to test ignoring or adding all mpi communication
    if (rank == 0) {
        // Rank 0 sends data
        int send_data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        printf("Rank 0: Sending data.\n");
        MPI_Send(send_data, 10, MPI_INT, 1, 2, MPI_COMM_WORLD);

    } else if (rank == 1) {
        // Rank 1 receives data
        int recv_data[10];
        MPI_Recv(recv_data, 10, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Rank 1: Received data: ");
        for (int j = 0; j < 10; j++) {
            printf("%d ", recv_data[j]);
        }
        printf("\n");
        
    }
    
    MPI_Finalize();
    
    return 0;
}