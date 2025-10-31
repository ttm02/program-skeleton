
set(USECASE_ALLOC_TRACKING_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(alloc_tracking_compiler_pass)
add_subdirectory(alloc_tracking_runtime_lib)
add_subdirectory(tests)

add_custom_target(clang_wrap_cc-alloc-tracking ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc && sed -i '3s/.*/compiler=clang/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(clang_wrap_cxx-alloc-tracking ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx && sed -i '3s/.*/compiler=clang++/' ${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(flang_wrap-alloc-tracking ALL
        COMMAND cp ${CMAKE_SOURCE_DIR}/clang_wrapper.sh ${CMAKE_CURRENT_BINARY_DIR}/flang_wrap && sed -i '3s/.*/compiler=flang/' ${CMAKE_CURRENT_BINARY_DIR}/flang_wrap
        DEPENDS ${CMAKE_SOURCE_DIR}/clang_wrapper.sh)

add_custom_target(setup_env.sh-alloc-tracking ALL
        COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/setup_env.sh.in ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=${PRECOMPUTE_LIB_PATH}:\\$$LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=${PRECOMPUTE_LIB_PATH}:\\$$LD_LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export ALLOC_TRACKING_PASS=$<TARGET_FILE:alloc_tracking_compiler_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export COMPILER_PASS=$<TARGET_FILE:alloc_tracking_compiler_pass>" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export USE_COMPILER_PASS=true" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CC=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cc" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export CLANG_WRAP_CXX=${CMAKE_CURRENT_BINARY_DIR}/clang_wrap_cxx" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/alloc_tracking_runtime_lib:\\$$LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh &&
        echo "export LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}/alloc_tracking_runtime_lib:\\$$LD_LIBRARY_PATH" >> ${CMAKE_CURRENT_BINARY_DIR}/setup_env.sh
        DEPENDS setup_env.sh.in)


