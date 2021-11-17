# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

include(${SRC_DIR}/../cmake/test_helpers.cmake)

setup()

execute(0 ${EXAMPLES_DIR}/basic/basic)

cleanup()
