cmake_minimum_required(VERSION 3.2)

project(dekaf2-benchmarks)

if(${CMAKE_VERSION} VERSION_GREATER "3.7.99")
	set(CMAKE_CXX_STANDARD 17)
	#set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()


# keep alphabetic sort order
set (SOURCES
	kbitfields_bench.cpp
	kcasestring_bench.cpp
	kprops_bench.cpp
	kreader_bench.cpp
	kstring_bench.cpp
	kstringview_bench.cpp
	kwriter_bench.cpp
	main.cpp
	other_bench.cpp
	shared_ptr_bench.cpp
	std_string_bench.cpp
)

set (HEADERS
)

add_executable(benchmarks ${HEADERS} ${SOURCES})
target_link_libraries(benchmarks dekaf2)
target_compile_definitions(benchmarks PRIVATE DEKAF2_ENABLE_PROFILING)

set(ALL_INSTALLABLE_TARGETS
	benchmarks
)

#install(TARGETS ${ALL_INSTALLABLE_TARGETS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})


