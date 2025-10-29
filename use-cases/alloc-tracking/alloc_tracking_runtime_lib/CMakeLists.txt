
# MPI is required by the runtime (it directly uses MPI calls)
find_package(MPI REQUIRED)

add_library(alloc_tracking_runtime_lib SHARED
        src/allocation_instrumentation.cpp
)

target_compile_features(alloc_tracking_runtime_lib PRIVATE cxx_std_17)

# includes: build vs install interface
target_include_directories(alloc_tracking_runtime_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# export the MPI usage macro to the runtime compilation
target_compile_definitions(alloc_tracking_runtime_lib PRIVATE USE_MPI)

# compile flags
target_compile_options(alloc_tracking_runtime_lib PRIVATE
    -fno-rtti -Wall -Wextra -Wno-unused-parameter -O3 -g
)

# link mpi
target_link_libraries(alloc_tracking_runtime_lib PRIVATE MPI::MPI_CXX)
set_target_properties(alloc_tracking_runtime_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Install headers for downstream users (the top-level CMake will export/install the target)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)
endif()
