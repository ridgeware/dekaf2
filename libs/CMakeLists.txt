cmake_minimum_required(VERSION 3.2)

# project(libs)

# add the format lib
add_subdirectory(fmt-4.0.0)

# add fast regular expressions
if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_CXX_COMPILER_ID MATCHES "[cC][lL][aA][nN][gG]"))
	# re2 has a number of missing field initializers and unused parameters.
	# we assume this is on purpose and disable the warning / error.
	set(CXX_FLAGS_STASHED ${CMAKE_CXX_FLAGS})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-implicit-fallthrough -Wno-missing-field-initializers -Wno-unused-parameter")
endif()
add_subdirectory(re2)
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_CXX_FLAGS ${CXX_FLAGS_STASHED})
endif()
