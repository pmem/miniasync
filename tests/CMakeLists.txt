#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

include(cmake/ctest_helpers.cmake)

set(LIBS_BASIC
	uasync
	${CMAKE_THREAD_LIBS_INIT})

set(SOURCES_DUMMY_TEST
	dummy.c)

add_custom_target(tests)

add_flag(-Wall)

add_cstyle(tests-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch])
add_check_whitespace(tests-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
		${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.cmake
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.cmake)

# add tests directories
add_subdirectory(dummy)
add_subdirectory(ex_basic)
