# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${TEST_DIR}/memcpy_threads)
execute_assert_pass(${TEST_DIR}/memcpy_threads)

cleanup()
