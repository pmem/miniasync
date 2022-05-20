# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/runtime_test)

execute_assert_pass(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/runtime_test)

cleanup()
