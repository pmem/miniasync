#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

# add compiler flags using macro defined in functions.cmake file
add_flag(-Wall)
add_flag(-Wpointer-arith)
add_flag(-Wsign-compare)
add_flag(-Wunreachable-code-return)
add_flag(-Wmissing-variable-declarations)
add_flag(-fno-common)
add_flag(-Wunused-macros)
add_flag(-Wsign-conversion)

add_flag(-ggdb DEBUG)
add_flag(-DDEBUG DEBUG)

add_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" RELEASE)

add_cstyle(examples-all ${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch])
add_check_whitespace(examples-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
		${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt)

# add basic example
add_subdirectory(basic)
