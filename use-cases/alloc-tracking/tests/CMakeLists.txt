function(add_compare_test SRC_DIR)
    get_filename_component(TEST_NAME ${SRC_DIR} NAME_WE) # Extract filename without extension

    add_test(NAME "alloc-${TEST_NAME}"
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/compare.sh ${USECASE_ALLOC_TRACKING_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}
    )
endfunction()


function(add_mpi_compare_test SRC_DIR MPI_MODE NUMPROCS)
    get_filename_component(TEST_NAME ${SRC_DIR} NAME_WE) # Extract filename without extension

    add_test(NAME "alloc-${TEST_NAME}"
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/compare.sh ${USECASE_ALLOC_TRACKING_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR} USE_MPI ${MPI_MODE} ${NUMPROCS}
    )
    set_tests_properties("alloc-${TEST_NAME}" PROPERTIES PROCESSORS ${NUMPROCS})
endfunction()

add_compare_test(000_direct_call_constant)
add_compare_test(001_direct_call)
add_compare_test(002_direct_call_var)
add_compare_test(003_function)
add_compare_test(004_function_param)
add_compare_test(005_wrapper_function)
add_compare_test(006_wrapper_function_macro)
add_compare_test(007_wrapper_function_macro2)
#TODO testcase not portable as execution dir is hardcoded
#add_compare_test(008_from_file)
add_compare_test(009_from_cmd_args)
add_compare_test(010_nested_function)
add_compare_test(011_struct_data)
add_compare_test(012_struct)
add_compare_test(013_function_recursive)
add_compare_test(014_pointer)
add_compare_test(015_function_pointer)
add_compare_test(016_if)
add_compare_test(017_for_loop)
add_compare_test(018_recursive_tree)
add_compare_test(019_linked_list)
add_compare_test(020_preprocessor)
add_compare_test(021_realloc)
add_compare_test(022_calloc)
add_compare_test(023_aligned_alloc)
add_compare_test(024_switch)
add_compare_test(025_while_loop)
add_compare_test(026_from_function)
add_compare_test(027_zero_bytes)
add_compare_test(028_multiple_allocations)
add_compare_test(029_global_var)
add_compare_test(030_function_pointer_complex)
add_mpi_compare_test(031_mpi_bcast_ignore_mpi_comm IGNORE_MPI_COMMUNICATION 2)
add_mpi_compare_test(032_mpi_bcast_add_all_mpi_comm ADD_ALL_MPI_COMMUNICATION 2)
add_mpi_compare_test(033_mpi_send_recv_ignore_mpi_comm IGNORE_MPI_COMMUNICATION 2)
add_mpi_compare_test(034_mpi_send_recv_add_all_mpi_comm ADD_ALL_MPI_COMMUNICATION 2)
#TODO testcase not portable as execution dir is hardcoded
#add_compare_test(035_c_functions)
add_compare_test(036_heat_eq)
add_compare_test(037_matrix)
add_compare_test(038_function_ptr_2)
add_compare_test(039_triple_pointer_needs_inline)
add_compare_test(039_triple_pointer_needs_inline2)
#TODO testcase is known to fail, needs an explicit pointer initialization (see testcase 041)
#add_compare_test(040_n_nody_problem)
add_compare_test(041_n_body_problem_isolate)
add_mpi_compare_test(042_poisson_mpi IGNORE_MPI_COMMUNICATION 2)
add_mpi_compare_test(043_wave_mpi ADD_ALL_MPI_COMMUNICATION 2)
add_compare_test(044_walsh_haar)
add_compare_test(045_hypersphere_monte_carlo)
add_compare_test(046_dependent_allocations)
add_mpi_compare_test(047_mpi_comm_dependent_allocations ADD_ALL_MPI_COMMUNICATION 2)
add_mpi_compare_test(048_mpi_heat_eq_solver ADD_ALL_MPI_COMMUNICATION 8)
add_mpi_compare_test(048_mpi_heat_eq_solver2 ADD_ALL_MPI_COMMUNICATION 8)
add_mpi_compare_test(048_mpi_heat_eq_solver3 ADD_ALL_MPI_COMMUNICATION 8)
add_mpi_compare_test(048_mpi_heat_eq_solver4 ADD_ALL_MPI_COMMUNICATION 8)
add_mpi_compare_test(048_mpi_heat_eq_solver5 ADD_ALL_MPI_COMMUNICATIONv)
add_mpi_compare_test(048_mpi_heat_eq_solver6 ADD_ALL_MPI_COMMUNICATION 8)
add_compare_test(049_thesis_example_function_ptr)
add_compare_test(050_thesis_example_required_allocation)
add_compare_test(051_thesis_example_implementation)
add_compare_test(052_thesis_example_aborts)
# fortran testcase
add_test(NAME "alloc-053_simple_fortran"
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/053_simple_fortran/compare.sh ${USECASE_ALLOC_TRACKING_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/053_simple_fortran
)





