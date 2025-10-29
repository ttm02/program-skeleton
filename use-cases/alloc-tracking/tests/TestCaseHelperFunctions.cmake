# TestCaseHelperFunctions.cmake

function(setup_test_case)
    set(options IGNORE_MPI_COMMUNICATION ADD_ALL_MPI_COMMUNICATION USE_MPI)
    set(oneValueArgs OPTIMIZATION_LEVEL)
    set(multiValueArgs SOURCES WRAPPER_FUNCTIONS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    get_filename_component(TEST_CASE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    message(STATUS "\tSetup of test case: ${TEST_CASE_NAME}")

    # Default source file
    if(ARG_SOURCES)
        set(SOURCES ${ARG_SOURCES})
    else()
        set(SOURCES main.c)
    endif()

    add_executable(${TEST_CASE_NAME} ${SOURCES})

    # Determine plugin path expression or explicit path
    if(TARGET AllocTrackerLTOPass)
        # in-tree target
        set(PASS_PLUGIN_EXPR "$<TARGET_FILE:AllocTrackerLTOPass>")
        # ensure plugin will be built before the test (parallel-safe)
        add_dependencies(${TEST_CASE_NAME} AllocTrackerLTOPass)
    elseif(TARGET AllocTracking::AllocTrackerLTOPass)
        # imported installed target
        set(PASS_PLUGIN_EXPR "$<TARGET_FILE:AllocTracking::AllocTrackerLTOPass>")
    elseif(DEFINED ALLOCTRACKING_PLUGIN_PATH AND ALLOCTRACKING_PLUGIN_PATH)
        set(PASS_PLUGIN_EXPR "${ALLOCTRACKING_PLUGIN_PATH}")
    else()
        message(FATAL_ERROR "Cannot find pass plugin. Either build plugin in-tree or install package and pass -DAllocTracking_DIR=/install/path/lib/cmake/AllocTracking or set -DALLOCTRACKING_PLUGIN_PATH=/path/to/libAllocTrackerLTOPass.so")
    endif()

    # If test uses MPI, link MPI and append pass-specific mllvm flags
    set(PASS_EXTRA_FLAGS "")
    if(ARG_USE_MPI)
        find_package(MPI REQUIRED)
        target_link_libraries(${TEST_CASE_NAME} PRIVATE MPI::MPI_C)

        if(ARG_IGNORE_MPI_COMMUNICATION)
            message(STATUS "\tIgnoring MPI communication => only flagging problematic allocation call.")
            list(APPEND PASS_EXTRA_FLAGS "-Wl,-mllvm=-at-pass-ignore-mpi-communication=true")
        endif()

        if(ARG_ADD_ALL_MPI_COMMUNICATION)
            message(STATUS "\tAdding all MPI communication")
            list(APPEND PASS_EXTRA_FLAGS "-Wl,-mllvm=-at-pass-add-all-mpi-communication=true")
        endif()
    endif()

    # Handle wrapper functions list
    if(ARG_WRAPPER_FUNCTIONS)
        # Join the function names with commas
        string(JOIN "," WRAPPER_FUNCTIONS_STR ${ARG_WRAPPER_FUNCTIONS})
        list(APPEND PASS_EXTRA_FLAGS "-Wl,-mllvm=-at-pass-allocation-wrapper-functions=${WRAPPER_FUNCTIONS_STR}")
        message(STATUS "\tWrapper functions: ${WRAPPER_FUNCTIONS_STR}")
    else()
        # Pass empty string to indicate no wrapper functions
        list(APPEND PASS_EXTRA_FLAGS "-Wl,-mllvm=-at-pass-allocation-wrapper-functions=")
        message(STATUS "\tNo allocation wrapper functions specified.")
    endif()

    # Compose compile and link flags for this test
    set(LTO_COMPILE_FLAGS -flto -fwhole-program-vtables -fno-inline)
    if(CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        list(APPEND LTO_COMPILE_FLAGS -g)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RELEASE")
        # Use custom optimization level if provided, otherwise default to -O2
        if(ARG_OPTIMIZATION_LEVEL)
            list(APPEND LTO_COMPILE_FLAGS ${ARG_OPTIMIZATION_LEVEL})
            message(STATUS "\tUsing custom optimization level: ${ARG_OPTIMIZATION_LEVEL}")
        else()
            list(APPEND LTO_COMPILE_FLAGS -O2)
        endif()
    endif()

    set(LTO_LINK_FLAGS
        -flto
        -fuse-ld=lld
        "-Wl,--load-pass-plugin=${PASS_PLUGIN_EXPR}"
        "-Wl,-mllvm=-load=${PASS_PLUGIN_EXPR}"
        "-lm" # c math lib
    )

    # Enable/disable slicing & logging via options passed to test-suite configure
    if(DEFINED ENABLE_SLICING AND ENABLE_SLICING)
        list(APPEND LTO_LINK_FLAGS "-Wl,-mllvm=-at-pass-enable-slicing=true")
    else()
        list(APPEND LTO_LINK_FLAGS "-Wl,-mllvm=-at-pass-enable-slicing=false")
    endif()

    if(DEFINED ENABLE_LOGGING AND ENABLE_LOGGING)
        list(APPEND LTO_LINK_FLAGS "-Wl,-mllvm=-at-pass-enable-logging=true")
    else()
        list(APPEND LTO_LINK_FLAGS "-Wl,-mllvm=-at-pass-enable-logging=false")
    endif()

    list(APPEND LTO_LINK_FLAGS ${PASS_EXTRA_FLAGS})

    # Apply compile and link options to the test executable
    target_compile_options(${TEST_CASE_NAME} PRIVATE ${LTO_COMPILE_FLAGS})
    # target_link_options accepts generator expressions (PASS_PLUGIN_EXPR might be one)
    target_link_options(${TEST_CASE_NAME} PRIVATE ${LTO_LINK_FLAGS})

    # Link runtime library (support both in-tree and installed exported target)
    if(TARGET AllocTrackerRuntimeLib)
        target_link_libraries(${TEST_CASE_NAME} PRIVATE AllocTrackerRuntimeLib)
        target_include_directories(${TEST_CASE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../alloc_tracking_runtime_lib)
    elseif(TARGET AllocTracking::AllocTrackerRuntimeLib)
        target_link_libraries(${TEST_CASE_NAME} PRIVATE AllocTracking::AllocTrackerRuntimeLib)
    else()
        message(FATAL_ERROR "Could not find AllocTrackerRuntimeLib. Build runtime lib in-tree or install the package and point CMake to it.")
    endif()

    message(STATUS "\tFULL COMPILE OPTIONS: ${LTO_COMPILE_FLAGS}")
    message(STATUS "\tFULL LINK OPTIONS: ${LTO_LINK_FLAGS}")
endfunction()
