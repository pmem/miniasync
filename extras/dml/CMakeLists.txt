#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

project(miniasync-dml C)

set(MINIASYNC_DML_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_cstyle(miniasync-dml
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_check_whitespace(miniasync-dml
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

set(DML_SOURCES
    vdm_dml.c
)

add_library(miniasync-dml STATIC ${DML_SOURCES})

target_include_directories(miniasync-dml PRIVATE .
	${MINIASYNC_DML_INCLUDE_DIR}
	${MINIASYNC_INCLUDE_DIR}
	${MINIASYNC_SOURCE_DIR})

set_target_properties(miniasync-dml PROPERTIES
	PUBLIC_HEADER ${MINIASYNC_DML_INCLUDE_DIR}/libminiasync-dml.h
)

install(TARGETS miniasync-dml
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install headers included in public header
install(DIRECTORY ${MINIASYNC_DML_INCLUDE_DIR}/libminiasync-dml
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING PATTERN "*.h"
)
