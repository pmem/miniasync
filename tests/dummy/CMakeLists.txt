#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

add_test_executable(dummy
		"${SOURCES_DUMMY_TEST}"
        "${LIBS_BASIC}")

configure_test("dummy" test_dummy memcheck)
