include(${CMAKE_SOURCE_DIR}/cmake/TestCaseHelperFunctions.cmake)


setup_test_case(
    USE_MPI
    IGNORE_MPI_COMMUNICATION
    #ADD_ALL_MPI_COMMUNICATION
    SOURCES main.c
    WRAPPER_FUNCTIONS hypre_MAlloc
    OPTIMIZATION_LEVEL "-O2" 
)

# We ignore MPI Communication here, 
# i.e. the respective malloc call that depends on the MPI_Send/MPI_Recv will be marked as invalid
