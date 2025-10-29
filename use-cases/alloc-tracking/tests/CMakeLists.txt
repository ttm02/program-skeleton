function(add_compare_test SRC_DIR)
    get_filename_component(TEST_NAME ${SRC_DIR} NAME_WE) # Extract filename without extension

    add_test(NAME "alloc-${TEST_NAME}"
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/compare.sh ${USECASE_ALLOC_TRACKING_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_DIR}
    )
endfunction()

add_compare_test(000_direct_call_constant)
add_compare_test(001_direct_call)