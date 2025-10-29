include(${CMAKE_SOURCE_DIR}/cmake/TestCaseHelperFunctions.cmake)


setup_test_case(
    #USE_MPI
    #IGNORE_MPI_COMMUNICATION
    #ADD_ALL_MPI_COMMUNICATION
    SOURCES main.c fd1d_heat_explicit.c
    WRAPPER_FUNCTIONS

    # This test case cannot be compiled with the default -O2,
    #   the slicing does not work... ptr poison issue, slicing lib cannot handle it?
    OPTIMIZATION_LEVEL "-O0" 
)
