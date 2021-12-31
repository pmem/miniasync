# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# test for the basic-async example

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${EXAMPLES_DIR}/example-basic-async)

cleanup()
