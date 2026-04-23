add_test(
    NAME testbench
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh
                 $<TARGET_FILE:compiler>
                 $<TARGET_FILE:vm>
                 ${CMAKE_CURRENT_SOURCE_DIR}
                 ${CMAKE_SOURCE_DIR}/res/langs
)
