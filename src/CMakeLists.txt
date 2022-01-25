#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
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
    runtime.c
    vdm.c
)

set(CORE_DEPS
	${CORE_SOURCE_DIR}/alloc.c
	${CORE_SOURCE_DIR}/os_posix.c
	${CORE_SOURCE_DIR}/os_thread_posix.c
	${CORE_SOURCE_DIR}/out.c
	${CORE_SOURCE_DIR}/util.c
	${CORE_SOURCE_DIR}/util_posix.c)

add_library(miniasync SHARED ${SOURCES} ${CORE_DEPS})

target_include_directories(miniasync PRIVATE . include)

set_target_properties(miniasync PROPERTIES
	PUBLIC_HEADER ${MINIASYNC_INCLUDE_DIR}/libminiasync.h
)

install(TARGETS miniasync
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install headers included in public header
install(DIRECTORY ${MINIASYNC_INCLUDE_DIR}/libminiasync
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
