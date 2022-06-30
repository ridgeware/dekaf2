cmake_minimum_required(VERSION 3.2)

project(dekaf2-smoketests)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (dekaf2-smoketests_IS_TOP_LEVEL)
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
target_link_libraries(dekaf2-smoketests dekaf2 ksql2)

