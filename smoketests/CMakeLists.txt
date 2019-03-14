cmake_minimum_required(VERSION 3.2)

project(dekaf2-smoketests)

if(${CMAKE_VERSION} VERSION_GREATER "3.7.99")
	set(CMAKE_CXX_STANDARD 17)
	# do not require cpp17
	#set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
)

add_executable(dekaf2-smoketests EXCLUDE_FROM_ALL ${UHEADERS} ${USOURCES})
target_link_libraries(dekaf2-smoketests dekaf2)

