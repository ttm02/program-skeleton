include(${CMAKE_SOURCE_DIR}/cmake/TestCaseHelperFunctions.cmake)

setup_test_case(
    #USE_MPI
    #IGNORE_MPI_COMMUNICATION
    #ADD_ALL_MPI_COMMUNICATION
    SOURCES main.c
    WRAPPER_FUNCTIONS memory_alloc
    OPTIMIZATION_LEVEL "-O2" 
)