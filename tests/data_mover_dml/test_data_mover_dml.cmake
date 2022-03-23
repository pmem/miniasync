# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

# an example for the data_mover_dml test case

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

# check for MOVDIR64B instruction
execute_process(COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/check_movdir64b
		RESULT_VARIABLE RET)

# execute DML tests only if MOVDIR64B instruction is available
if (RET EQUAL 0)
	execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/data_mover_dml)
else()
	message("Wrong return from check_movdir64b: ${RET}")
endif()

cleanup()
