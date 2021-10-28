#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

cmake_minimum_required(VERSION 3.3)

project(libuasync C)

set(LIBUASYNC_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/)
set(LIBUASYNC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/include/)

include(FindPerl)
include(FindThreads)
include(CMakePackageConfigHelpers)
include(CheckCCompilerFlag)
include(GNUInstallDirs)
include(${CMAKE_SOURCE_DIR}/cmake/functions.cmake)

option(BUILD_EXAMPLES "build examples" ON)

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

# add CMakeLists.txt from the examples directory
if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()
