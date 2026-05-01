add_library(precomputation_compiler_lib SHARED
        debug.cpp
        analysis_results.cpp
        precalculations.cpp
        devirt_analysis.cpp
        ptr_info.cpp
        precompute_backend_funcs.cpp
        Precompute_insertion.cpp
        VtableManager.cpp
        mpi_functions.cpp
        precalculation_function_analysis.cpp
        std_funcs.cpp
        openmp_runtime_functions.cpp
        Openmp_region.cpp
)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
cmake_minimum_required(VERSION 3.30)
find_package(Boost 1.66.0 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(precomputation_compiler_lib ${Boost_LIBRARIES})

# if one wants to use mpi
#find_package(MPI REQUIRED)
#target_link_libraries(mpi_assertion_checker PRIVATE MPI::MPI_C)


#TODO document that the pass only builds with clang

# -fsanitize=address
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -g -Og -DDEBUG_MACH_PASS=1 -gdwarf-4")
# db on my local platform has some issues with gdwarf 5

#TODO the link step does not work correctly, we still need to preload the libasan
#TODO check if compiler is clang
#if (CMAKE_BUILD_TYPE MATCHES DEBUG)
#    add_link_options(precomputation_compiler_lib "-fsanitize=address -shared-libasan")
#endif (CMAKE_BUILD_TYPE MATCHES DEBUG)
# switch off one Warning for release build: some variables are only used for assertions
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DDEBUG_MACH_PASS=0 -Wno-unused-but-set-variable")

# Use C++17 to compile our pass (i.e., supply -std=c++11).
target_compile_features(precomputation_compiler_lib PRIVATE cxx_std_17)

# LLVM is (typically) built with no C++ RTTI. We need to match that;
# otherwise, we'll get linker errors about missing RTTI data.
set_target_properties(precomputation_compiler_lib PROPERTIES
        COMPILE_FLAGS "-fno-rtti -Wall -Wextra -Wno-unused-parameter"
)

