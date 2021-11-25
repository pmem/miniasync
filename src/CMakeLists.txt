#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

add_cstyle(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_check_whitespace(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

set(SOURCES
    future.c
    mover.c
    runtime.c
)

add_library(miniasync SHARED ${SOURCES})

target_include_directories(miniasync PRIVATE . include)

set_target_properties(miniasync PROPERTIES
	PUBLIC_HEADER ${MINIASYNC_INCLUDE_DIR}/miniasync.h
)
