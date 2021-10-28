#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

include_directories(
    ${LIBUASYNC_SOURCE_DIR}
    ${LIBUASYNC_INCLUDE_DIR}
    )

set(srcs 
    basic.c
    ${LIBUASYNC_SOURCE_DIR}/future.c
    ${LIBUASYNC_SOURCE_DIR}/mover.c
    ${LIBUASYNC_SOURCE_DIR}/runtime.c
)

add_executable(basic ${srcs})

target_link_libraries (basic ${CMAKE_THREAD_LIBS_INIT})
