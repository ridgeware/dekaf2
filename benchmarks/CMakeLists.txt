cmake_minimum_required(VERSION 3.13)

project(dekaf2-benchmarks)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_REQUIRED_QUIET TRUE)

# older cmake versions do not support the test for IS_TOP_LEVEL
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(dekaf2-benchmarks_IS_TOP_LEVEL ON)
else()
	set(dekaf2-benchmarks_IS_TOP_LEVEL OFF)
endif()

if (dekaf2-benchmarks_IS_TOP_LEVEL OR NOT DEKAF2_GENERATED_FILES_DIR)
	set(DEKAF2_INHERIT_BUILD_FLAGS ON)
	find_package(dekaf2 2.0 REQUIRED)
	set(DEKAF2_NAMESPACE "dekaf2::")
else()
	set(DEKAF2_NAMESPACE)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(simdutf QUIET)

if(simdutf_FOUND)
	set(DEKAF2_HAS_SIMDUTF 1)
	set(DEKAF2_SIMDUTF simdutf::simdutf)
endif()

find_package(Iconv QUIET)

if (Iconv_FOUND)
	set(DEKAF2_HAS_ICONV 1)
	# Iconv_LIBRARIES may be empty, then iconv is built into libc
	set(DEKAF2_ICONV "${Iconv_LIBRARIES}")
endif()

find_package(utf8cpp QUIET)

if(utf8cpp_FOUND)
	set(DEKAF2_HAS_UTF8CPP 1)
	set(DEKAF2_UTF8CPP utf8cpp::utf8cpp)
endif()

# keep alphabetic sort order
set (SOURCES
	kxml_bench.cpp
	khtml_bench.cpp
	kbitfields_bench.cpp
	kcasestring_bench.cpp
	kfindsetofchars_bench.cpp
	kprops_bench.cpp
	kreader_bench.cpp
	kstring_bench.cpp
	kstringview_bench.cpp
	kutf_bench.cpp
	kwriter_bench.cpp
	main.cpp
	other_bench.cpp
	shared_ptr_bench.cpp
	std_string_bench.cpp
)

set (HEADERS
)

add_executable(benchmarks EXCLUDE_FROM_ALL ${HEADERS} ${SOURCES})
target_link_libraries(benchmarks ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} ${DEKAF2_ICONV} ${DEKAF2_SIMDUTF} ${DEKAF2_UTF8CPP} Threads::Threads)
target_compile_definitions(benchmarks PRIVATE DEKAF2_ENABLE_PROFILING)
if (DEKAF2_HAS_SIMDUTF)
	target_compile_definitions(benchmarks PRIVATE DEKAF2_HAS_SIMDUTF)
endif()
if (DEKAF2_HAS_ICONV)
	target_compile_definitions(benchmarks PRIVATE DEKAF2_HAS_ICONV)
endif()
if (DEKAF2_HAS_UTF8CPP)
	target_compile_definitions(benchmarks PRIVATE DEKAF2_HAS_UTF8CPP)
endif()

add_executable(faststrings EXCLUDE_FROM_ALL fast_kstring_bench.cpp)
target_link_libraries(faststrings ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} Threads::Threads)
target_compile_definitions(faststrings PRIVATE DEKAF2_ENABLE_PROFILING)

add_executable(restserver EXCLUDE_FROM_ALL restserver.cpp)
target_link_libraries(restserver ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} Threads::Threads)

set(ALL_INSTALLABLE_TARGETS
	benchmarks
)

#install(TARGETS ${ALL_INSTALLABLE_TARGETS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})


