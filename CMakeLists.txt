#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

cmake_minimum_required(VERSION 3.3)

project(miniasync C)

set(MINIASYNC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/)
set(MINIASYNC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/include/)

option(COVERAGE "run coverage test" OFF)
option(DEVELOPER_MODE "enable developer checks" OFF)
option(CHECK_CSTYLE "check code style of C sources" OFF)
option(USE_ASAN "enable AddressSanitizer (debugging)" OFF)
option(USE_UBSAN "enable UndefinedBehaviorSanitizer (debugging)" OFF)

include(FindPerl)
include(FindThreads)
include(CMakePackageConfigHelpers)
include(CheckCCompilerFlag)
include(GNUInstallDirs)
include(${CMAKE_SOURCE_DIR}/cmake/functions.cmake)

# look for pkg config (use it later for managing valgrind)
find_package(PkgConfig REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

if(NOT PERL_FOUND)
	message(FATAL_ERROR "Perl not found")
endif()

add_custom_target(checkers ALL)
add_custom_target(cstyle)
add_custom_target(check-whitespace)
add_custom_target(check-license
	COMMAND ${CMAKE_SOURCE_DIR}/utils/check_license/check-headers.sh
		${CMAKE_SOURCE_DIR}
		BSD-3-Clause)

add_custom_target(check-commits
	COMMAND ${CMAKE_SOURCE_DIR}/utils/check-commits.sh)

add_custom_target(check-whitespace-main
	COMMAND ${PERL_EXECUTABLE}
		${CMAKE_SOURCE_DIR}/utils/check_whitespace
		${CMAKE_SOURCE_DIR}/utils/check_license/*
		${CMAKE_SOURCE_DIR}/README.md)

add_dependencies(check-whitespace check-whitespace-main)

# add compiler flags using macro defined in the functions.cmake file
add_flag(-Wpointer-arith)
add_flag(-Wunused-macros)
add_flag(-Wsign-conversion)
add_flag(-Wsign-compare)
add_flag(-Wunreachable-code-return)
add_flag(-Wmissing-variable-declarations)
add_flag(-fno-common)
add_flag(-std=gnu99)
add_flag(-ggdb DEBUG)
add_flag(-DDEBUG DEBUG)
add_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" RELEASE)

if(USE_ASAN)
	add_sanitizer_flag(address)
endif()

if(USE_UBSAN)
	add_sanitizer_flag(undefined)
endif()

if(COVERAGE)
       set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -coverage")
endif()

if(DEVELOPER_MODE)
	add_flag(-Wall)
	add_flag(-Werror)

	if(CHECK_CSTYLE)
		add_dependencies(checkers cstyle)
	endif()
	add_dependencies(checkers check-whitespace)
	add_dependencies(checkers check-license)
	add_dependencies(checkers check-commits)
endif()

# add checkers for the root CMakeLists file and for cmake helper files
add_check_whitespace(cmake-main ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)
add_check_whitespace(cmake-helpers ${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.cmake)

# add CMakeLists.txt from the src directory
add_subdirectory(src)
