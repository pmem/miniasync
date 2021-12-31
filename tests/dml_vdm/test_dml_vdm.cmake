# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

# an example for the dml_vdm test case

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${TEST_DIR}/dml_vdm)

cleanup()
