cmake_minimum_required(VERSION 3.13...3.31)

project(dekaf2-smoketests)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# older cmake versions do not support the test for IS_TOP_LEVEL
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	set(dekaf2-smoketests_IS_TOP_LEVEL ON)
else()
	set(dekaf2-smoketests_IS_TOP_LEVEL OFF)
endif()

if (dekaf2-smoketests_IS_TOP_LEVEL OR NOT DEKAF2_GENERATED_FILES_DIR)
	set(DEKAF2_INHERIT_BUILD_FLAGS ON)
	find_package(dekaf2 2.0 REQUIRED)
	set(DEKAF2_NAMESPACE "dekaf2::")
else()
	set(DEKAF2_NAMESPACE)
endif()

set (UHEADERS
	catch.hpp
)

set (USOURCES
	main.cpp
	khttpclient_smoketests.cpp
	kinshell_smoketests.cpp
	ktlsstream_smoketests.cpp
	ksql_smoketests.cpp
	kresolve_smoketests.cpp
	ksystem_smoketests.cpp
)

add_executable(dekaf2-smoketests EXCLUDE_FROM_ALL ${UHEADERS} ${USOURCES})
target_link_libraries(dekaf2-smoketests PRIVATE ${DEKAF2_NAMESPACE}dekaf2${DEKAF2_LINK_SHARED} ${DEKAF2_NAMESPACE}ksql2${DEKAF2_LINK_SHARED})
