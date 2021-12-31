#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

add_cstyle(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.[ch])

add_check_whitespace(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.[ch])

set(SOURCES
    future.c
    vdm.c
    runtime.c
)

if(WIN32)
	set(SOURCES ${SOURCES} core/os_thread_windows.c)
	set(SOURCES ${SOURCES} core/os_windows.c)
	set(SOURCES ${SOURCES} core/util_windows.c)
	set(SOURCES ${SOURCES} core/util.c)
	set(SOURCES ${SOURCES} core/out.c)
	set(SOURCES ${SOURCES} core/alloc.c)
else()
	set(SOURCES ${SOURCES} core/os_thread_posix.c)
	set(SOURCES ${SOURCES} core/os_posix.c)
	set(SOURCES ${SOURCES} core/util.c)
	set(SOURCES ${SOURCES} core/util_posix.c)
	set(SOURCES ${SOURCES} core/alloc.c)
	set(SOURCES ${SOURCES} core/out.c)
	set(SOURCES ${SOURCES} core/ringbuf.c)

endif()
message(STATUS "MY_VARIABLE=${SOURCES}")
add_library(miniasync STATIC ${SOURCES})

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
