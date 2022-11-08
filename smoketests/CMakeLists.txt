cmake_minimum_required(VERSION 3.2)

project(dekaf2-smoketests)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# older cmake versions do not support the test for IS_TOP_LEVEL
if (dekaf2-smoketests_IS_TOP_LEVEL OR NOT GENERATED_FILES_DIR)
	set(DEKAF2_INHERIT_BUILD_FLAGS ON)
	find_package(dekaf2 2.0 REQUIRED)
endif()

set (UHEADERS
	catch.hpp
)

set (USOURCES
	main.cpp
	khttpclient_smoketests.cpp
	kinshell_smoketests.cpp
	ksslstream_smoketests.cpp
	ksql_smoketests.cpp
	ksystem_smoketests.cpp
)

add_executable(dekaf2-smoketests EXCLUDE_FROM_ALL ${UHEADERS} ${USOURCES})
target_include_directories(dekaf2-smoketests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(dekaf2-smoketests dekaf2 ksql2)

if (DEKAF2_USE_PRECOMPILED_HEADERS)
	target_precompile_headers(dekaf2-smoketests REUSE_FROM dekaf2)
endif()
