include(${CMAKE_SOURCE_DIR}/cmake/TestCaseHelperFunctions.cmake)


setup_test_case(
    USE_MPI
    #IGNORE_MPI_COMMUNICATION
    ADD_ALL_MPI_COMMUNICATION
    SOURCES main.c
    WRAPPER_FUNCTIONS
    OPTIMIZATION_LEVEL "-O2" 
)

# We DO NOT ignore MPI Communication here but add all MPI communication to the slice, 
# i.e. if one MPI_Bcast is needed for a dynamic alloation call, then ALL MPI_Bcast calls will be kept in the slice.
