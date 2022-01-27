# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

# test for the basic example

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/example-basic)

cleanup()
