add_library(alloc_tracking_compiler_pass MODULE
        # List your source files here.
        src/alloc_tracker_lto_pass.cpp
        src/logging.cpp
        src/utils.cpp
        src/io.cpp
)

set_target_properties(alloc_tracking_compiler_pass PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

target_compile_features(alloc_tracking_compiler_pass PRIVATE cxx_std_17)


# Add the precompute program slicing libraries
target_link_libraries(alloc_tracking_compiler_pass PRIVATE precomputation_compiler_lib)
target_include_directories(alloc_tracking_compiler_pass PRIVATE ${CMAKE_SOURCE_DIR}/precomputation_compiler_lib)


target_include_directories(alloc_tracking_compiler_pass PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)


target_compile_options(alloc_tracking_compiler_pass PRIVATE -fno-rtti -Wall -Wextra -Wno-unused-parameter -O3)

#set_target_properties(alloc_tracking_compiler_pass PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)
endif()
