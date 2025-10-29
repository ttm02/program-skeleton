function(add_compare_test SRC_FILE)
    get_filename_component(TEST_NAME ${SRC_FILE} NAME_WE) # Extract filename without extension

    add_test(NAME ${TEST_NAME}
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/compare.sh ${USECASE_SANITIZER_BINARY_DIR} ${SRC_FILE}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES PROCESSORS 2)
endfunction()


add_test(NAME alloc_1
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/compare.sh ${USECASE_ALLOC_TRACKING_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/000_direct_call_constant/main.c
)
