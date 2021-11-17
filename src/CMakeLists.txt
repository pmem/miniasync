#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

add_cstyle(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_check_whitespace(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)

set(SOURCES
    future.c
    mover.c
    runtime.c
)

add_library(uasync SHARED ${SOURCES})

target_include_directories(uasync PRIVATE . include)

set_target_properties(uasync PROPERTIES
	PUBLIC_HEADER ${LIBUASYNC_INCLUDE_DIR}/libuasync.h
)
