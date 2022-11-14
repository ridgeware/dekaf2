cmake_minimum_required(VERSION 3.8)

# project(from)

if(DEKAF2_HAS_MINIFOLLY)
	# add facebook folly extracts
	if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
		# folly has a number of unused parameters.
		# we assume this is on purpose and disable the warning / error.
		set(CXX_FLAGS_STASHED ${CMAKE_CXX_FLAGS})
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
	endif()
	add_subdirectory(folly)
	if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
		set(CMAKE_CXX_FLAGS ${CXX_FLAGS_STASHED})
	endif()
endif()

add_subdirectory(nlohmann)
