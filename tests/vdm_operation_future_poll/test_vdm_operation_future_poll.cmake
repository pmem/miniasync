# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

# check for MOVDIR64B instruction
execute_process(COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/check_movdir64b
        RESULT_VARIABLE RET)

# execute DML tests only if MOVDIR64B instruction is available
if (RET EQUAL 0)
    execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/vdm_operation_future_poll)
else()
    message("Wrong return from check_movdir64b: ${RET}")
endif()

cleanup()
