# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

set(GLOBAL_TEST_ARGS -DPARENT_DIR=${TEST_DIR}/)

if(TRACE_TESTS)
	set(GLOBAL_TEST_ARGS ${GLOBAL_TEST_ARGS} --trace-expand)
endif()

# add and link an executable
function(add_test_executable name sources libs)
	add_executable(${name} ${sources})

	target_include_directories(${name}
		PRIVATE ${LIBUASYNC_SOURCE_DIR}
		${LIBUASYNC_INCLUDE_DIR})

	target_link_libraries(${name} PRIVATE ${libs})
endfunction()

set(vg_tracers memcheck helgrind drd)

# add and configure test ${name}
function(configure_test name file tracer)
	if (${tracer} IN_LIST vg_tracers)
			if (NOT TESTS_USE_VALGRIND)
				message(STATUS
					"Tests using Valgrind are switched off, skipping Valgrind test: ${name}")
				return()
			endif()
			if (NOT VALGRIND_FOUND)
				message(WARNING
					"Valgrind not found, skipping Valgrind test: ${name}")
				return()
			endif()
			if (COVERAGE_BUILD)
				message(STATUS
					"This is the Coverage build, skipping Valgrind test: ${name}")
				return()
			endif()
			if (USE_ASAN OR USE_UBSAN)
				message(STATUS
					"Sanitizer used, skipping Valgrind test: ${name}")
				return()
			endif()
	endif()
	add_test(NAME ${name}
		COMMAND ${CMAKE_COMMAND}
		${GLOBAL_TEST_ARGS}
		-DTEST_NAME=${name}
		-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
		-DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}/${file}
		-DCONFIG=$<CONFIG>
		-DTRACER=${tracer}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/${file}.cmake)

	set_tests_properties(${name} PROPERTIES
		ENVIRONMENT "LC_ALL=C;PATH=$ENV{PATH}"
		TIMEOUT 300)
endfunction()
