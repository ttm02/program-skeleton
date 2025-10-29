cmake_minimum_required(VERSION 3.20)
project(alloctracking_tests LANGUAGES C CXX)

option(ENABLE_SLICING "Enable slicing for complete test suite." OFF)
option(ENABLE_LOGGING "Enable logging for complete test suite." OFF)
option(ALLOCTRACKING_PLUGIN_PATH "Explicit path to installed libAllocTrackerLTOPass.so (fallback)" "")

if(NOT TARGET AllocTrackerLTOPass AND NOT TARGET AllocTracking::AllocTrackerLTOPass)
    find_package(AllocTracking CONFIG QUIET)
endif()

# common flags
set(TEST_SUITE_LTO_COMPILE_FLAGS "-flto" "-fwhole-program-vtables" "-fno-inline" CACHE INTERNAL "Common LTO Compile Flags")
set(TEST_SUITE_LTO_LINK_FLAGS "" CACHE INTERNAL "Common LTO Link Flags for Pass")

# Load helper function
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(TestCaseHelperFunctions)

# Discover and add all test case subdirs under tests/
set(TEST_CASES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
file(GLOB TEST_CASE_DIRS LIST_DIRECTORIES true RELATIVE ${TEST_CASES_DIR} "${TEST_CASES_DIR}/*")
foreach(dir ${TEST_CASE_DIRS})
    if(IS_DIRECTORY "${TEST_CASES_DIR}/${dir}" AND EXISTS "${TEST_CASES_DIR}/${dir}/CMakeLists.txt"
        # NOTE: add or remove specific test cases here
        AND NOT "${dir}" STREQUAL "040_n_nody_problem" AND NOT "${dir}" STREQUAL "048_mpi_heat_eq_solver4"
        AND NOT "${dir}" STREQUAL "048_mpi_heat_eq_solver5" AND NOT "${dir}" STREQUAL "048_mpi_heat_eq_solver6"
        AND NOT "${dir}" STREQUAL "052_thesis_example_aborts"
        #AND "${dir}" STREQUAL "050_thesis_example_required_allocation"
        )

        message(STATUS "Adding Test Case: ${TEST_CASES_DIR}/${dir}")
        add_subdirectory(${TEST_CASES_DIR}/${dir})
    endif()
endforeach()
