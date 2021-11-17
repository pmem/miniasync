#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

include_directories(
    ${LIBUASYNC_SOURCE_DIR}
    ${LIBUASYNC_INCLUDE_DIR}
    )

set(SOURCES
    basic.c
)

add_executable(basic ${SOURCES})

target_link_libraries (basic
    uasync
    ${CMAKE_THREAD_LIBS_INIT}
)
