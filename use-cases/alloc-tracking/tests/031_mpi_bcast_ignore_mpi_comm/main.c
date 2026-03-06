#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE 10
#define MESSAGE_SIZE 256

int main(int argc, char** argv) {
    int rank, size;
    int data[ARRAY_SIZE];
    char message[MESSAGE_SIZE];
    double params[39];
    
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    // Get process rank and total number of processes
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    printf("Process %d of %d started\n", rank, size);
    
    // Root process (rank 0) initializes the data
    if (rank == 0) {
        printf("Root process initializing data...\n");
        
        // Initialize integer array
        for (int i = 0; i < ARRAY_SIZE; i++) {
            data[i] = i * i;  // Square numbers
        }
        
        // Initialize message string
        strcpy(message, "Hello from root process!");
        
        // Initialize broadcast value
        params[0] = 3.14159;
        
        printf("Root process data initialized:\n");
        printf("  Array: ");
        for (int i = 0; i < ARRAY_SIZE; i++) {
            printf("%d ", data[i]);
        }
        printf("\n  Message: %s\n", message);
        printf("  Broadcast value: %.5f\n", params[0]);
    }
    
    // Broadcast integer array from root to all processes
    MPI_Bcast(data, ARRAY_SIZE, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Broadcast message string from root to all processes
    MPI_Bcast(message, MESSAGE_SIZE, MPI_CHAR, 0, MPI_COMM_WORLD);
    
    // Broadcast double value from root to all processes
    MPI_Bcast(params, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    double broadcast_value = params[0];
    
    // Calculate allocation size based on broadcasted value
    int allocation_size = (int)(broadcast_value * 1000) + rank * 100;
    
    // Allocate memory based on broadcasted data
    double* dynamic_array = (double*)malloc(allocation_size * sizeof(double));
    double* dynamic_array2 = (double*)malloc(allocation_size * 2 * sizeof(double));
    double* dynamic_array3 = (double*)malloc(2 * sizeof(double));
    
    // Initialize the allocated memory with some values
    for (int i = 0; i < allocation_size; i++) {
        dynamic_array[i] = broadcast_value * i + rank;
    }
    
    printf("Process %d allocated %d doubles (%.2f KB) based on broadcast value\n", 
           rank, allocation_size, (allocation_size * sizeof(double)) / 1024.0);
    
    // All processes print received data
    printf("\nProcess %d received broadcast data:\n", rank);
    printf("  Array: ");
    for (int i = 0; i < ARRAY_SIZE; i++) {
        printf("%d ", data[i]);
    }
    printf("\n  Message: %s\n", message);
    printf("  Broadcast value: %.5f\n", broadcast_value);
    
    // Demonstrate some computation on broadcasted data
    int local_sum = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
        local_sum += data[i];
    }
    
    printf("Process %d computed local sum: %d\n", rank, local_sum);
    
    // Synchronize all processes before finishing
    MPI_Barrier(MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("\nAll processes completed successfully!\n");
    }
    
    // Free allocated memory
    free(dynamic_array);
    
    // Finalize MPI
    MPI_Finalize();
    
    return 0;
}